# JUCE Plugin Building Memory

## Current Distribution Stage

- Build and test plugins as VST3 and AU.
- Build Standalone when useful for quick UI/audio checks.
- Do not prioritize AAX yet. Add AAX once the plugins are ready for professional distribution and the Avid AAX license/build flow is in place.

## PatchWork / Pro Tools Menu Compatibility

When building JUCE plugin UIs, assume the plugins may be hosted in nested chains such as:

`Pro Tools -> Blue Cat PatchWork -> VST3/AU plugin`

In that environment, knobs/sliders can work while pull-down menus fail, because JUCE popup menus and ComboBox dropdowns may create transient popup windows that do not receive focus correctly inside the wrapper.

Default UI rule:

- Prefer in-editor, parented popup/dropdown behavior.
- Avoid detached/native popup menu behavior inside plugin editors.
- Be suspicious of native title bars, desktop peers, native menus, or UI elements that create separate OS-level windows.

For custom `juce::PopupMenu` usage, prefer options like:

```cpp
juce::PopupMenu::Options()
    .withTargetComponent(&button)
    .withParentComponent(getTopLevelComponent());
```

or, from inside the editor:

```cpp
juce::PopupMenu::Options()
    .withTargetComponent(&button)
    .withParentComponent(this);
```

For `juce::ComboBox`, use a shared LookAndFeel or custom dropdown control so menus stay attached to the plugin editor rather than becoming independent host-level popups.

## Test Matrix Before Calling A Plugin Stable

- VST3 in a normal host.
- AU in a normal host.
- Standalone, if available.
- VST3/AU inside Blue Cat PatchWork standalone.
- VST3/AU inside Blue Cat PatchWork in Pro Tools.

If controls work but menus do not, treat it as a JUCE popup/dropdown containment issue first.
