import random
import json


def generate_musicgen_prompt(template, params):
    """
    Génère un prompt pour MusicGen en remplissant un template

    Args:
        template (str): Template du prompt
        params (dict): Paramètres à insérer

    Returns:
        str: Prompt complet
    """
    # Substituer les paramètres dans le template
    prompt = template

    for key, value in params.items():
        placeholder = "{" + key + "}"
        prompt = prompt.replace(placeholder, str(value))

    return prompt


def enhance_music_description(base_description, genre, intensity):
    """
    Enrichit une description musicale avec des termes spécifiques

    Args:
        base_description (str): Description de base
        genre (str): Genre musical
        intensity (int): Niveau d'intensité (1-10)

    Returns:
        str: Description enrichie
    """
    # Dictionnaires de termes spécifiques par genre
    genre_terms = {
        "techno": [
            "driving",
            "hypnotic",
            "four-on-the-floor",
            "warehouse",
            "industrial",
            "analog",
            "pulsating",
            "berlin",
            "detroit",
            "minimal",
            "acid",
            "rave",
            "trance-inducing",
        ],
        "house": [
            "groovy",
            "soulful",
            "disco-influenced",
            "jackin",
            "deep",
            "funky",
            "vocal",
            "piano",
            "uplifting",
            "garage",
            "chicago",
            "filter",
            "looped",
        ],
        "ambient": [
            "atmospheric",
            "ethereal",
            "spacious",
            "dreamy",
            "floating",
            "textural",
            "lush",
            "cinematic",
            "evolving",
            "soundscape",
            "glacial",
            "haunting",
            "droning",
        ],
        "experimental": [
            "abstract",
            "glitchy",
            "deconstructed",
            "avant-garde",
            "noisy",
            "distorted",
            "unconventional",
            "fractured",
            "chaotic",
            "granular",
            "generative",
            "modular",
            "algorithmic",
        ],
    }

    # Termes liés à l'intensité
    intensity_low = [
        "subtle",
        "gentle",
        "restrained",
        "delicate",
        "soft",
        "understated",
    ]
    intensity_mid = [
        "balanced",
        "moderate",
        "steady",
        "consistent",
        "flowing",
        "measured",
    ]
    intensity_high = [
        "intense",
        "powerful",
        "energetic",
        "driving",
        "hard",
        "peak-time",
    ]

    # Sélectionner les termes selon l'intensité
    if intensity <= 3:
        intensity_terms = intensity_low
    elif intensity <= 7:
        intensity_terms = intensity_mid
    else:
        intensity_terms = intensity_high

    # Sélectionner aléatoirement des termes
    genre_specific = random.sample(
        genre_terms.get(genre, genre_terms["techno"]),
        min(3, len(genre_terms.get(genre, genre_terms["techno"]))),
    )
    intensity_specific = random.sample(intensity_terms, min(2, len(intensity_terms)))

    # Combiner les termes
    all_terms = genre_specific + intensity_specific
    random.shuffle(all_terms)

    # Ajouter à la description
    enhanced = f"{base_description}, {', '.join(all_terms)}"

    return enhanced


def parse_llm_response(response_text):
    """
    Analyse la réponse du LLM pour extraire le JSON

    Args:
        response_text (str): Réponse du LLM

    Returns:
        dict: Dictionnaire extrait de la réponse JSON
    """
    # Essayer de trouver un objet JSON dans la réponse
    import re

    json_match = re.search(r"({.*})", response_text, re.DOTALL)

    if json_match:
        try:
            # Parser le JSON
            json_str = json_match.group(1)
            return json.loads(json_str)
        except json.JSONDecodeError:
            pass

    # Pas de JSON valide trouvé, créer une structure par défaut
    return {
        "action_type": "sample",
        "parameters": {"type": "techno_kick", "intensity": 5},
        "reasoning": "Décision par défaut - pas de JSON valide détecté",
    }
