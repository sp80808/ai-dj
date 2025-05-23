def create_vst_system_prompt(style="techno_minimal", include_stems=False) -> str:
    """Prompt VST qui s'adapte vraiment aux demandes custom"""

    # Récupérer les paramètres du style (mais juste comme référence)
    if style not in VST_STYLE_PARAMS:
        style = "techno_minimal"

    style_params = VST_STYLE_PARAMS[style]
    default_tempo = style_params.get("default_tempo", 126)
    default_key = style_params.get("default_key", "C minor")

    json_example = f"""
{{
    "action_type": "generate_sample",
    "parameters": {{
        "sample_details": {{
            "type": "auto_detect",
            "musicgen_prompt_keywords": ["deep", "punchy", "resonant"],
            "key": "{default_key}",
            "measures": 2,
            "intensity": 7
        }}
    }},
    "reasoning": "Explication brève de tes choix"
}}
"""

    return f"""Tu es DJ-IA VST, expert en génération de loops musicales TOUS STYLES CONFONDUS.

⚠️ ⚠️ ⚠️ RÈGLE ABSOLUE ⚠️ ⚠️ ⚠️

QUAND L'UTILISATEUR DEMANDE UN STYLE SPÉCIFIQUE, TU DOIS COMPLÈTEMENT ABANDONNER LE STYLE PAR DÉFAUT ({style}) ET T'ADAPTER À SA DEMANDE !

EXEMPLES CONCRETS D'ADAPTATION :
• User: "drum and bass beat" → type: "auto_detect", keywords: ["drum", "and", "bass", "breakbeat", "amen", "rolling"]
• User: "ambient space pad" → type: "auto_detect", keywords: ["ambient", "space", "pad", "atmospheric", "ethereal"]  
• User: "reggae dub bass" → type: "auto_detect", keywords: ["reggae", "dub", "bass", "echo", "steppers"]
• User: "rock guitar riff" → type: "auto_detect", keywords: ["rock", "guitar", "riff", "distorted", "powerful"]

PROCESSUS D'ADAPTATION :
1. LIS le prompt utilisateur dans "special_instruction" ou "user_request"
2. IDENTIFIE le genre/style demandé 
3. UTILISE "auto_detect" comme type (le système s'adaptera automatiquement)
4. INCLUS tous les mots-clés pertinents du genre demandé dans "musicgen_prompt_keywords"
5. AJUSTE l'intensité selon le style (ambient=faible, metal=élevée)

RÈGLES TECHNIQUES :
- Tempo par défaut : {default_tempo} BPM (mais adapte si le genre l'exige)
- Tonalité par défaut : {default_key} (mais adapte si nécessaire)
- Utilise TOUJOURS "auto_detect" comme type quand l'user demande un style spécifique
- Le système détectera automatiquement le bon type de sample selon tes mots-clés

INTENSITÉ PAR GENRE :
- Ambient/Chill : 3-5
- House/Techno : 6-7  
- Rock/Metal : 8-9
- Hip-hop/Trap : 6-8
- Drum & Bass : 8-9
- Dub/Reggae : 5-7

FORMAT JSON OBLIGATOIRE :
{json_example}

RAPPEL FINAL : Si l'utilisateur veut du drum and bass alors que tu es configuré en techno → GÉNÈRE DU DRUM AND BASS !
Le style par défaut ({style}) n'est qu'un fallback si aucune demande spécifique n'est faite.
"""


# Dictionnaire des paramètres par style
VST_STYLE_PARAMS = {
    "techno_minimal": {
        "default_measures": 1,
        "preferred_stems": ["drums", "bass"],
        "base_keywords": ["minimal", "deep", "driving", "analog", "hypnotic", "berlin"],
        "intensity_range": (6, 9),
        "sample_types": [
            "techno_kick",
            "techno_bass",
            "techno_synth",
            "techno_percussion",
            "techno_pad",
            "techno_fx",
        ],
        "default_tempo": 126,
        "default_key": "C minor",
    },
    "experimental": {
        "default_measures": 3,
        "preferred_stems": ["other", "drums"],
        "base_keywords": [
            "glitchy",
            "distorted",
            "granular",
            "metallic",
            "abstract",
            "noise",
            "avant-garde",
        ],
        "intensity_range": (7, 10),
        "sample_types": [
            "glitch_perc",
            "noise_synth",
            "abstract_bass",
            "drone_pad",
            "experimental_fx",
        ],
        "default_tempo": 130,
        "default_key": "atonal",
    },
    "rock": {
        "default_measures": 4,
        "preferred_stems": ["drums", "other"],
        "base_keywords": [
            "distorted",
            "aggressive",
            "powerful",
            "riff",
            "guitar",
            "energetic",
        ],
        "intensity_range": (7, 9),
        "sample_types": [
            "rock_drums",
            "rock_guitar",
            "rock_bass",
            "rock_synth",
            "rock_fx",
        ],
        "default_tempo": 120,
        "default_key": "E minor",
    },
    "hip_hop": {
        "default_measures": 2,
        "preferred_stems": ["drums", "other"],
        "base_keywords": ["boom bap", "trap", "lo-fi", "808"],
        "intensity_range": (5, 8),
        "sample_types": ["hiphop_beat", "hiphop_bass", "hiphop_melody", "hiphop_fx"],
        "default_tempo": 90,
        "default_key": "G minor",
    },
    "jungle_dnb": {
        "default_measures": 4,
        "preferred_stems": ["drums", "bass"],
        "base_keywords": [
            "amen break",
            "reese bass",
            "atmospheric",
            "rolling",
            "breakbeat",
            "jungle",
        ],
        "intensity_range": (7, 9),
        "sample_types": [
            "jungle_break",
            "jungle_bass",
            "jungle_pad",
            "jungle_fx",
            "dnb_percussion",
            "dnb_bass",
            "dnb_synth",
            "dnb_fx",
        ],
        "default_tempo": 174,
        "default_key": "F minor",
    },
    "dub": {
        "default_measures": 4,
        "preferred_stems": ["bass", "drums"],
        "base_keywords": [
            "deep",
            "echo",
            "steppers",
            "roots",
            "delay",
            "reggae",
            "space",
        ],
        "intensity_range": (5, 8),
        "sample_types": [
            "dub_riddim",
            "dub_bass",
            "dub_melody",
            "dub_fx",
            "dub_skanks",
            "dub_horns",
        ],
        "default_tempo": 70,
        "default_key": "D minor",
    },
    "deep_house": {
        "default_measures": 4,
        "preferred_stems": ["drums", "other"],
        "base_keywords": ["deep", "jazzy", "soulful", "warm", "organic", "melodic"],
        "intensity_range": (5, 8),
        "sample_types": [
            "house_kick",
            "house_bass",
            "house_chord",
            "house_pad",
            "house_percussion",
            "house_fx",
        ],
        "default_tempo": 124,
        "default_key": "E flat minor",
    },
    "classical": {
        "default_measures": 4,
        "preferred_stems": ["other", "drums"],
        "base_keywords": [
            "orchestral",
            "baroque",
            "romantic",
            "staccato",
            "legato",
            "cinematic",
        ],
        "intensity_range": (4, 7),
        "sample_types": [
            "classical_strings",
            "classical_piano",
            "classical_brass",
            "classical_percussion",
            "classical_choir",
            "classical_harp",
        ],
        "default_tempo": 110,
        "default_key": "D minor",
    },
    "downtempo_ambient": {
        "default_measures": 8,
        "preferred_stems": ["other"],
        "base_keywords": [
            "ethereal",
            "atmospheric",
            "spacious",
            "reverberant",
            "lush",
            "meditative",
            "chill",
        ],
        "intensity_range": (3, 6),
        "sample_types": [
            "ambient_pad",
            "ambient_texture",
            "ambient_drone",
            "ambient_bells",
            "ambient_drums",
            "ambient_bass",
            "field_recording",
        ],
        "default_tempo": 85,
        "default_key": "A minor",
    },
    "trip_hop": {
        "default_measures": 4,
        "preferred_stems": ["drums", "other"],
        "base_keywords": [
            "mellow",
            "cinematic",
            "vinyl",
            "scratchy",
            "melancholic",
            "downtempo",
            "nostalgic",
        ],
        "intensity_range": (4, 7),
        "sample_types": [
            "triphop_beat",
            "triphop_bass",
            "triphop_melody",
            "triphop_fx",
            "triphop_pad",
            "triphop_scratch",
        ],
        "default_tempo": 95,
        "default_key": "C minor",
    },
}

# Configuration des stems par type
VST_STEM_CONFIGS = {
    "rhythmic": ["drums"],
    "bass": ["bass"],
    "melodic": ["other"],
    "full": ["drums", "bass", "other"],
    "percussion": ["drums"],
    "atmospheric": ["other"],
    "effect": ["other"],
}
