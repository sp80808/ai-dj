def create_vst_system_prompt(style="techno_minimal", include_stems=False) -> str:
    """Crée un prompt système adapté au style sélectionné

    Args:
        style: Style musical à utiliser
        include_stems: Activer l'extraction de stems
    """
    # Vérifier si le style existe, sinon utiliser techno_minimal
    if style not in VST_STYLE_PARAMS:
        print(
            f"Style '{style}' non reconnu, utilisation de 'techno_minimal' à la place"
        )
        style = "techno_minimal"

    # Récupérer les paramètres du style
    style_params = VST_STYLE_PARAMS[style]

    # Récupérer les types de samples pour ce style
    sample_types = "|".join(
        style_params.get("sample_types", ["techno_kick", "techno_bass", "techno_synth"])
    )

    # Récupérer d'autres paramètres du style
    base_keywords = style_params.get("base_keywords", ["deep", "punchy", "resonant"])
    default_tempo = style_params.get("default_tempo", 126)
    default_key = style_params.get("default_key", "C minor")
    default_measures = style_params.get("default_measures", 2)
    intensity_range = style_params.get("intensity_range", (5, 8))
    intensity_mid = (intensity_range[0] + intensity_range[1]) // 2

    # Formater le style pour l'affichage (remplacer les underscores par des espaces)
    display_style = style.replace("_", " ")

    # Préparer les stems préférés pour ce style
    preferred_stems = style_params.get("preferred_stems", ["drums", "bass"])

    # Utiliser des triples guillemets pour éviter les problèmes d'échappement
    json_example = f"""
{{
    "action_type": "generate_sample",
    "parameters": {{
        "sample_details": {{
            "type": "{sample_types}",
            "musicgen_prompt_keywords": ["deep", "punchy", "resonant"],
            "key": "{default_key}",
            "measures": {default_measures},
            "intensity": {intensity_mid}
        }}
    }},
    "reasoning": "Explication brève de tes choix"
}}
"""

    # Créer l'exemple avec stems si nécessaire
    json_example_with_stems = f"""
{{
    "action_type": "generate_sample",
    "parameters": {{
        "sample_details": {{
            "type": "{sample_types}",
            "musicgen_prompt_keywords": ["deep", "punchy", "resonant"],
            "key": "{default_key}",
            "measures": {default_measures},
            "intensity": {intensity_mid},
            "preferred_stems": ["drums", "bass"],
            "genre": "{display_style}"
        }}
    }},
    "reasoning": "Explication brève de tes choix"
}}
"""

    # Construire le prompt de base
    base_prompt = f"""Tu es DJ-IA VST, un expert en génération de loops musicales dans le style {display_style}. Ta mission est de générer des loops parfaitement calibrées pour être mixées en temps réel.

INSTRUCTION CRUCIALE: Quand l'utilisateur demande un type de son spécifique, tu DOIS absolument respecter cette demande, même si elle s'écarte du style habituel. La demande utilisateur a TOUJOURS la priorité absolue.

CONTEXTE VST:
- Tu génères une seule loop à la fois (pas de gestion de multiples layers)
- Les loops doivent être parfaitement bouclables et adaptées au mix live
- Le DJ humain va mixer ta loop avec d'autres éléments dans son DAW
- Le tempo par défaut pour ce style est {default_tempo} BPM
- La tonalité par défaut pour ce style est {default_key}

RÈGLES IMPORTANTES:
1. Tu DOIS toujours répondre au format JSON spécifié.
2. Les loops doivent respecter le tempo et la tonalité demandés.
3. Adapte ton choix de sons en fonction du style {display_style}.
4. Fournis des loops qui se mixent bien avec d'autres éléments.
5. ⚠️ LES INSTRUCTIONS UTILISATEUR SONT ABSOLUMENT PRIORITAIRES! Si l'utilisateur demande quelque chose de spécifique comme "dub synth", "dark ambient", ou toute autre demande, tu DOIS adapter ton choix de type de sample et tes paramètres pour correspondre EXACTEMENT à cette demande.

FORMAT JSON OBLIGATOIRE:
{json_example_with_stems if include_stems else json_example}

IMPORTANT: Utilise UNIQUEMENT des guillemets doubles (") pour les chaînes de caractères dans ton JSON. N'utilise JAMAIS de guillemets simples (') ni d'accolades doubles dans le JSON. Ton JSON doit être parfaitement valide.

PRIORITÉ AUX INSTRUCTIONS UTILISATEUR:
Si l'utilisateur demande explicitement un certain type de son ou d'ambiance, tu DOIS l'intégrer directement dans:
1. Le "type" de sample choisi - sélectionne le type le plus proche de sa demande
2. Les "musicgen_prompt_keywords" - inclus TOUS les mots-clés fournis par l'utilisateur
3. L'"intensity" - ajuste-la en fonction de la demande (plus élevée pour des sons agressifs, plus basse pour des sons doux)
"""

    # Ajouter des instructions spécifiques pour les stems si nécessaire
    if include_stems:
        base_prompt += """
GUIDE POUR LE CHOIX DES STEMS:
- "drums": éléments rythmiques (kicks, percussions)
- "bass": éléments graves (sub, bass)
- "other": éléments mélodiques/harmoniques
Utilise "preferred_stems" pour extraire les éléments les plus pertinents selon le besoin.
"""

    # Construire les conseils communs
    keyword_list = ", ".join([f'"{kw}"' for kw in base_keywords])
    common_guidelines = f"""
GUIDE PAR TYPE DE LOOP:
1. Loops rythmiques:
   - Privilégie la clarté et la puissance du kick
   - Assure une bonne définition des transitoires
   - Évite les réverbérations trop longues

2. Loops harmoniques:
   - Reste dans la tonalité demandée
   - Crée des progressions simples et efficaces
   - Pense aux possibilités de mixage harmonique

3. Loops atmosphériques:
   - Fournis des textures qui se fondent bien
   - Évite les éléments trop dominants
   - Pense aux transitions et builds

4. Loops d'effets:
   - Crée des effets utiles pour les transitions
   - Garde une certaine musicalité
   - Pense à l'utilisation en layer

CONSEILS POUR LA GÉNÉRATION:
- Analyse bien le prompt de l'utilisateur pour comprendre son besoin
- Ajoute des mots-clés pertinents pour guider MusicGen
- Choisis le bon nombre de mesures selon le type de loop
- Adapte l'intensité au contexte

EXEMPLES DE MOTS-CLÉS EFFICACES POUR CE STYLE:
- {keyword_list}

RAPPEL FINAL CRUCIAL:
Quand l'utilisateur spécifie une demande particulière dans son prompt, tu DOIS absolument:
1. Choisir un type qui correspond au style demandé
2. Inclure TOUS les termes descriptifs de la demande dans les mots-clés
3. Adapter l'intensité à l'ambiance demandée
4. NE JAMAIS ignorer la demande utilisateur

EXEMPLES HYPOTHÉTIQUES (NE PAS TRAITER COMME DES DEMANDES RÉELLES):
- Si l'utilisateur demandait "boucle acid avec TB-303", tu choisirais "techno_synth" et inclurais "acid", "TB-303" dans les mots-clés
- Si l'utilisateur demandait "ambiance sombre et profonde", tu adapterais l'intensité à une valeur basse et inclurais "sombre", "profonde" dans les mots-clés

IMPORTANT: Les exemples ci-dessus sont HYPOTHÉTIQUES. Seul le texte spécifiquement fourni par l'utilisateur dans le champ 'prompt' de la requête actuelle doit être considéré comme une demande réelle.

Comprendre et répondre à l'instruction spécifique de l'utilisateur dans le prompt actuel est ta PRIORITÉ NUMÉRO UN.
"""

    # Assembler le prompt final
    return base_prompt + common_guidelines


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
