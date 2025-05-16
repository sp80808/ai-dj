# Templates de prompts pour MusicGen

# Recherche sur les prompts efficaces pour MusicGen
MUSICGEN_TEMPLATES = {
    "techno_kick": "A deep, punchy techno kick drum pattern with {tempo} BPM, minimal and driving, clean low end, perfect for mixing, no melody, just rhythm section, {style_tag}",
    "techno_bass": "A dark techno bassline at {tempo} BPM, hypnotic and repetitive with subtle variations, in key {key}, minimal elements, {style_tag}",
    "techno_synth": "A minimal techno synth pad at {tempo} BPM, atmospheric and hypnotic, in key {key}, subtle and evolving texture, {style_tag}",
    "techno_percussion": "Techno percussion loop at {tempo} BPM, crisp hi-hats and claps, no kick drum, just tops, minimal techno style, {style_tag}",
    "techno_fx": "Abstract techno sound effect, atmospheric and spatial, suitable for transitions in a {tempo} BPM techno track, {style_tag}",
}

# Paramètres spécifiques pour chaque type de sample
SAMPLE_PARAMS = {
    "techno_kick": {
        "duration": 8,  # 8 secondes = typiquement 4 mesures à 120 BPM
        "should_start_with_kick": True,
        "key_sensitive": False,
    },
    "techno_bass": {
        "duration": 8,
        "should_start_with_kick": False,
        "key_sensitive": True,
    },
    "techno_synth": {
        "duration": 16,  # Plus long pour les pads atmosphériques
        "should_start_with_kick": False,
        "key_sensitive": True,
    },
    "techno_percussion": {
        "duration": 8,
        "should_start_with_kick": False,
        "key_sensitive": False,
    },
    "techno_fx": {
        "duration": 4,
        "should_start_with_kick": False,
        "key_sensitive": False,
    },
}
