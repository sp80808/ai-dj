# AudioParameter Generator for DJ-IA VST
# Génère le code C++ pour tous les paramètres des 8 slots


def generate_parameters():
    print("// === Generated AudioParameters for DJ-IA VST ===")
    print("DjIaVstProcessor::DjIaVstProcessor()")
    print("    : AudioProcessor(createBusLayout()),")
    print('    parameters(*this, nullptr, "Parameters", {')

    # Paramètres existants
    print("        // Existing parameters")
    print(
        '        std::make_unique<juce::AudioParameterBool>("generate", "Generate Loop", false),'
    )
    print(
        '        std::make_unique<juce::AudioParameterBool>("play", "Play Loop", false),'
    )
    print(
        '        std::make_unique<juce::AudioParameterBool>("autoload", "Auto-Load", true),'
    )
    print(
        '        std::make_unique<juce::AudioParameterFloat>("bpm", "BPM", 60.0f, 200.0f, 126.0f),'
    )
    print()

    # Master Section
    print("        // Master Section")
    master_params = [
        ("masterVolume", "Master Volume", "Float", "0.0f, 1.0f, 0.8f"),
        ("masterPan", "Master Pan", "Float", "-1.0f, 1.0f, 0.0f"),
        ("masterHigh", "Master High EQ", "Float", "-12.0f, 12.0f, 0.0f"),
        ("masterMid", "Master Mid EQ", "Float", "-12.0f, 12.0f, 0.0f"),
        ("masterLow", "Master Low EQ", "Float", "-12.0f, 12.0f, 0.0f"),
    ]

    for param_id, param_name, param_type, param_range in master_params:
        print(
            f'        std::make_unique<juce::AudioParameter{param_type}>("{param_id}", "{param_name}", {param_range}),'
        )
    print()

    # 8 Slots
    slot_params = [
        ("Volume", "Volume", "Float", "0.0f, 1.0f, 0.8f"),
        ("Pan", "Pan", "Float", "-1.0f, 1.0f, 0.0f"),
        ("Mute", "Mute", "Bool", "false"),
        ("Solo", "Solo", "Bool", "false"),
        ("Play", "Play", "Bool", "false"),
        ("Stop", "Stop", "Bool", "false"),
        ("Generate", "Generate", "Bool", "false"),
        ("Pitch", "Pitch", "Float", "-12.0f, 12.0f, 0.0f"),
        ("Fine", "Fine", "Float", "-50.0f, 50.0f, 0.0f"),
        ("BpmOffset", "BPM Offset", "Float", "-20.0f, 20.0f, 0.0f"),
    ]

    for slot in range(1, 9):  # Slots 1-8
        print(f"        // Slot {slot}")
        for param_suffix, param_display, param_type, param_range in slot_params:
            param_id = f"slot{slot}{param_suffix}"
            param_name = f"Slot {slot} {param_display}"
            comma = "," if slot < 8 or param_suffix != "BpmOffset" else ""
            print(
                f'        std::make_unique<juce::AudioParameter{param_type}>("{param_id}", "{param_name}", {param_range}){comma}'
            )
        print()

    print("    })")
    print("{")
    print("    // Récupérer les pointeurs pour accès rapide")

    # Master params
    print("    // Master parameters")
    for param_id, _, _, _ in master_params:
        var_name = param_id + "Param"
        print(f'    {var_name} = parameters.getRawParameterValue("{param_id}");')
    print()

    # Slot params arrays
    print("    // Slot parameters arrays")
    for param_suffix, _, _, _ in slot_params:
        var_name = f"slot{param_suffix}Params"
        print(f"    for (int i = 0; i < 8; ++i)")
        print(f"    {{")
        print(f'        juce::String slotName = "slot" + juce::String(i + 1);')
        print(
            f'        {var_name}[i] = parameters.getRawParameterValue(slotName + "{param_suffix}");'
        )
        print(f"    }}")
        print()

    print("}")
    print()

    # Header declarations
    print("// === Header file declarations (add to PluginProcessor.h) ===")
    print("private:")

    # Master param pointers
    print("    // Master parameter pointers")
    for param_id, _, _, _ in master_params:
        var_name = param_id + "Param"
        print(f"    std::atomic<float>* {var_name} = nullptr;")
    print()

    # Slot param arrays
    print("    // Slot parameter arrays")
    for param_suffix, _, _, _ in slot_params:
        var_name = f"slot{param_suffix}Params"
        print(f"    std::atomic<float>* {var_name}[8] = {{nullptr}};")
    print()

    # ProcessBlock code
    print("// === ProcessBlock code ===")
    print("void DjIaVstProcessor::processBlock(...)")
    print("{")
    print("    // Apply parameters to tracks")
    print("    auto trackIds = trackManager.getAllTrackIds();")
    print("    for (const auto& trackId : trackIds)")
    print("    {")
    print("        if (TrackData* track = trackManager.getTrack(trackId))")
    print("        {")
    print("            int slot = track->slotIndex;")
    print("            if (slot >= 0 && slot < 8)")
    print("            {")
    print("                // Apply slot parameters to track")
    print("                track->volume = slotVolumeParams[slot]->load();")
    print("                track->pan = slotPanParams[slot]->load();")
    print("                track->isMuted = slotMuteParams[slot]->load() > 0.5f;")
    print("                track->isSolo = slotSoloParams[slot]->load() > 0.5f;")
    print("                track->fineOffset = slotFineParams[slot]->load();")
    print("                track->bpmOffset = slotBpmOffsetParams[slot]->load();")
    print("                // track->pitch handling here...")
    print()
    print("                // Handle button triggers (reset after use)")
    print("                if (slotGenerateParams[slot]->load() > 0.5f)")
    print("                {")
    print("                    slotGenerateParams[slot]->store(0.0f);")
    print("                    triggerGeneration(trackId);")
    print("                }")
    print()
    print("                if (slotPlayParams[slot]->load() > 0.5f)")
    print("                {")
    print("                    slotPlayParams[slot]->store(0.0f);")
    print("                    triggerPlay(trackId);")
    print("                }")
    print()
    print("                if (slotStopParams[slot]->load() > 0.5f)")
    print("                {")
    print("                    slotStopParams[slot]->store(0.0f);")
    print("                    triggerStop(trackId);")
    print("                }")
    print("            }")
    print("        }")
    print("    }")
    print()
    print("    // Apply master parameters")
    print("    float masterVol = masterVolumeParam->load();")
    print("    float masterPanVal = masterPanParam->load();")
    print("    // Apply master EQ...")
    print()
    print("    // ... rest of your processBlock")
    print("}")


if __name__ == "__main__":
    generate_parameters()
