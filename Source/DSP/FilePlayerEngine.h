#pragma once
#include <JuceHeader.h>

// Loads and plays audio files, routing their output through the same buffer
// that processBlock receives. When a file is loaded, it replaces the live input.
// Thread-safe: AudioTransportSource handles audio/UI thread cross-calls internally.
class FilePlayerEngine
{
public:
    FilePlayerEngine() : readAheadThread("TidelineReadAhead")
    {
        formatManager.registerBasicFormats();
        readAheadThread.startThread(juce::Thread::Priority::normal);
    }

    ~FilePlayerEngine()
    {
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
    bool         fileLoaded        = false;
    double       currentSampleRate = 48000.0;
};
