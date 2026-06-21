#pragma once
#include <JuceHeader.h>
#include "LufsEngine.h"
#include <atomic>
#include <mutex>
#include <thread>
#include <vector>

// Loads and plays audio files, routing their output through the same buffer
// that processBlock receives. When a file is loaded, it replaces the live input.
// Thread-safe: AudioTransportSource handles audio/UI thread cross-calls internally.
class FilePlayerEngine
{
public:
    struct AlbumTrackResult
    {
        juce::String name;
        float integratedLUFS = -70.0f;
        float truePeakDB = -144.0f;
        float lra = 0.0f;
    };
    FilePlayerEngine() : readAheadThread("TidelineReadAhead")
    {
        formatManager.registerBasicFormats();
        readAheadThread.startThread(juce::Thread::Priority::normal);
    }

    ~FilePlayerEngine()
    {
        cancelAlbumAnalysis.store(true);
        if (albumAnalysisThread.joinable()) albumAnalysisThread.join();
        transportSource.setSource(nullptr);
        readAheadThread.stopThread(500);
    }

    void prepare(double sampleRate, int blockSize)
    {
        currentSampleRate = sampleRate;
        transportSource.prepareToPlay(blockSize, sampleRate);
    }

    void releaseResources()
    {
        transportSource.releaseResources();
    }

    // Call from the UI thread. Returns true if the file was loaded successfully.
    bool loadFile(const juce::File& file)
    {
        playlist.clear();
        playlist.add(file);
        return loadPlaylistFile(0);
    }

    // Standalone workflow: a dropped folder becomes an ordered album queue.
    bool loadFiles(const juce::Array<juce::File>& files)
    {
        playlist.clear();
        for (const auto& file : files)
        {
            if (file.isDirectory())
            {
                juce::Array<juce::File> children;
                file.findChildFiles(children, juce::File::findFiles, true);
                for (const auto& child : children) if (isAudioFile(child)) playlist.add(child);
            }
            else if (isAudioFile(file))
            {
                playlist.add(file);
            }
        }
        playlist.sort();
        return loadPlaylistFile(0);
    }

    bool nextFile()     { return loadPlaylistFile(playlistIndex + 1); }
    bool previousFile() { return loadPlaylistFile(playlistIndex - 1); }
    bool hasNextFile() const { return playlistIndex + 1 < playlist.size(); }
    bool hasPreviousFile() const { return playlistIndex > 0; }
    int getPlaylistSize() const { return playlist.size(); }
    int getPlaylistIndex() const { return playlistIndex; }

    void analyseAlbumAsync()
    {
        if (playlist.isEmpty() || albumAnalysing.exchange(true)) return;
        cancelAlbumAnalysis.store(false);
        if (albumAnalysisThread.joinable()) albumAnalysisThread.join();
        const auto files = playlist;
        {
            const std::scoped_lock lock(albumResultMutex);
            albumResults.clear();
        }
        albumAnalysisThread = std::thread([this, files]
        {
            juce::AudioFormatManager manager;
            manager.registerBasicFormats();
            for (const auto& file : files)
            {
                if (cancelAlbumAnalysis.load()) break;
                AlbumTrackResult result;
                result.name = file.getFileNameWithoutExtension();
                if (auto reader = std::unique_ptr<juce::AudioFormatReader>(manager.createReaderFor(file)))
                {
                    LufsEngine meter;
                    constexpr int blockSize = 8192;
                    meter.prepare(reader->sampleRate, blockSize);
                    juce::AudioBuffer<float> buffer(juce::jmin(2, (int)reader->numChannels), blockSize);
                    for (juce::int64 pos = 0; pos < reader->lengthInSamples && !cancelAlbumAnalysis.load(); pos += blockSize)
                    {
                        const int count = (int)juce::jmin<juce::int64>(blockSize, reader->lengthInSamples - pos);
                        buffer.setSize(buffer.getNumChannels(), count, false, false, true);
                        reader->read(&buffer, 0, count, pos, true, buffer.getNumChannels() > 1);
                        meter.process(buffer);
                    }
                    result.integratedLUFS = meter.getIntegratedLUFS();
                    result.truePeakDB = meter.getTruePeakDB();
                    result.lra = meter.getLRA();
                }
                const std::scoped_lock lock(albumResultMutex);
                albumResults.push_back(result);
            }
            albumAnalysing.store(false);
        });
    }

    bool isAnalysingAlbum() const { return albumAnalysing.load(); }
    std::vector<AlbumTrackResult> getAlbumResults() const
    {
        const std::scoped_lock lock(albumResultMutex);
        return albumResults;
    }

    float getTidalAlbumGainDB() const
    {
        const auto results = getAlbumResults();
        float loudest = -70.0f;
        for (const auto& result : results) loudest = juce::jmax(loudest, result.integratedLUFS);
        return loudest <= -69.0f ? 0.0f : juce::jmin(0.0f, -14.0f - loudest);
    }

    static bool isAudioFile(const juce::File& file)
    {
        const auto ext = file.getFileExtension().toLowerCase();
        return ext == ".wav" || ext == ".aiff" || ext == ".aif" || ext == ".mp3"
            || ext == ".flac" || ext == ".ogg" || ext == ".m4a" || ext == ".aac";
    }

    bool loadPlaylistFile(int index)
    {
        if (index < 0 || index >= playlist.size()) return false;
        const auto& file = playlist.getReference(index);
        transportSource.stop();
        transportSource.setSource(nullptr);
        readerSource.reset();

        auto* reader = formatManager.createReaderFor(file);
        if (reader == nullptr) { fileLoaded = false; return false; }

        readerSource = std::make_unique<juce::AudioFormatReaderSource>(reader, true);
        transportSource.setSource(readerSource.get(), 32768, &readAheadThread, reader->sampleRate);
        transportSource.setPosition(0.0);
        currentFile = file;
        fileLoaded  = true;
        playlistIndex = index;
        return true;
    }

    // Called on the audio thread from processBlock.
    // Clears the buffer and fills it with file audio (or silence if at end).
    void process(juce::AudioBuffer<float>& buffer)
    {
        if (!fileLoaded) return;

        juce::AudioSourceChannelInfo info(buffer);
        buffer.clear();
        transportSource.getNextAudioBlock(info);
    }

    void play()  { if (fileLoaded) transportSource.start(); }
    void stop()  { transportSource.stop(); }
    void rewind(){ transportSource.setPosition(0.0); }

    bool isPlaying()  const { return transportSource.isPlaying(); }
    bool hasFile()    const { return fileLoaded; }

    double getPositionSeconds() const { return transportSource.getCurrentPosition(); }
    double getLengthSeconds()   const { return transportSource.getLengthInSeconds(); }

    juce::String getFileName() const
    {
        return fileLoaded ? currentFile.getFileName() : juce::String{};
    }

    // True when playback reached the end of file
    bool isAtEnd() const
    {
        return fileLoaded && !transportSource.isPlaying()
               && transportSource.getCurrentPosition() >= transportSource.getLengthInSeconds() - 0.05;
    }

private:
    juce::TimeSliceThread                        readAheadThread;
    juce::AudioFormatManager                     formatManager;
    juce::AudioTransportSource                   transportSource;
    std::unique_ptr<juce::AudioFormatReaderSource> readerSource;

    juce::File   currentFile;
    juce::Array<juce::File> playlist;
    int           playlistIndex      = -1;
    bool         fileLoaded        = false;
    double       currentSampleRate = 48000.0;
    mutable std::mutex albumResultMutex;
    std::vector<AlbumTrackResult> albumResults;
    std::thread albumAnalysisThread;
    std::atomic<bool> albumAnalysing { false };
    std::atomic<bool> cancelAlbumAnalysis { false };
};
