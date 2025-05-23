from core.keyword_cleaner import KeywordCleaner


class GenreDetector:
    """Détecteur de genre centralisé pour simplifier la logique"""

    GENRE_MAPPING = {
        "drum_and_bass": {
            "keywords": [
                "drum and bass",
                "dnb",
                "jungle",
                "breakbeat",
                "amen break",
                "liquid",
                "neurofunk",
                "rolling",
            ],
            "sample_types": ["dnb_beat", "jungle_break", "dnb_bass", "dnb_synth"],
            "template": "A {intensity} drum and bass track at {tempo} BPM, {key}, with rolling breaks and deep sub bass",
            "default_intensity": 8,
        },
        "hip_hop": {
            "keywords": [
                "hip hop",
                "hip-hop",
                "trap",
                "boom bap",
                "rap",
                "beats",
                "808",
            ],
            "sample_types": ["hiphop_beat", "hiphop_bass", "hiphop_melody"],
            "template": "A {intensity} hip-hop beat at {tempo} BPM, {key}, with punchy drums and deep 808s",
            "default_intensity": 6,
        },
        "ambient": {
            "keywords": [
                "ambient",
                "atmospheric",
                "ethereal",
                "drone",
                "space",
                "chillout",
                "meditative",
            ],
            "sample_types": ["ambient_pad", "ambient_texture", "ambient_drone"],
            "template": "A {intensity} ambient soundscape at {tempo} BPM, {key}, atmospheric and spacious",
            "default_intensity": 4,
        },
        "dub": {
            "keywords": ["dub", "reggae", "steppers", "roots", "echo", "delay"],
            "sample_types": ["dub_riddim", "dub_bass", "dub_melody"],
            "template": "A {intensity} dub reggae track at {tempo} BPM, {key}, with deep echo and steppers rhythm",
            "default_intensity": 6,
        },
        "rock": {
            "keywords": ["rock", "guitar", "metal", "punk", "distorted", "heavy"],
            "sample_types": ["rock_drums", "rock_guitar", "rock_bass"],
            "template": "A {intensity} rock track at {tempo} BPM, {key}, with powerful drums and distorted guitars",
            "default_intensity": 8,
        },
        "house": {
            "keywords": [
                "house",
                "deep house",
                "disco",
                "four on the floor",
                "chicago",
            ],
            "sample_types": ["house_kick", "house_bass", "house_chord"],
            "template": "A {intensity} house track at {tempo} BPM, {key}, with four-on-the-floor groove",
            "default_intensity": 6,
        },
        "techno": {
            "keywords": ["techno", "minimal", "berlin", "acid", "industrial"],
            "sample_types": ["techno_kick", "techno_bass", "techno_synth"],
            "template": "A {intensity} techno track at {tempo} BPM, {key}, minimal and driving",
            "default_intensity": 7,
        },
    }

    @staticmethod
    def choose_sample_type_from_keywords(
        genre, cleaned_keywords, vst_style_params=None
    ):
        """
        Choisit intelligemment le sample_type basé sur les mots-clés et le style VST
        """
        # Récupérer les sample types disponibles pour ce genre
        if vst_style_params and genre in vst_style_params:
            available_types = vst_style_params[genre]["sample_types"]
        else:
            # Fallback sur les types par défaut du GENRE_MAPPING
            config = GenreDetector.GENRE_MAPPING.get(genre, {})
            available_types = config.get("sample_types", [f"{genre}_kick"])

        print(f"🎯 Choix du sample type pour {genre}:")
        print(f"   Types disponibles: {available_types}")
        print(f"   Mots-clés: {cleaned_keywords}")

        # Mapping COMPLET des mots-clés vers types de samples
        keyword_to_type = {
            # === TECHNO MINIMAL ===
            "techno_kick": [
                "kick",
                "beat",
                "rhythm",
                "four-on-the-floor",
                "four",
                "on",
                "floor",
            ],
            "techno_bass": ["bass", "sub", "bassline", "deep", "analog", "kick", "rhythm","four-on-the-floor"],
            "techno_synth": ["synth", "lead", "acid", "303", "melody", "keys"],
            "techno_percussion": [
                "percussion",
                "hi-hat",
                "hihat",
                "tops",
                "perc",
                "clap",
                "snare",
            ],
            "techno_pad": ["pad", "atmosphere", "ambient", "hypnotic", "space"],
            "techno_fx": ["fx", "sweep", "noise", "effect", "transition", "whoosh"],
            # === EXPERIMENTAL ===
            "glitch_perc": [
                "glitch",
                "percussion",
                "digital",
                "broken",
                "stuttering",
                "perc",
            ],
            "noise_synth": [
                "noise",
                "synth",
                "harsh",
                "industrial",
                "static",
                "distorted",
            ],
            "abstract_bass": [
                "abstract",
                "bass",
                "modulated",
                "granular",
                "weird",
                "processed",
            ],
            "drone_pad": [
                "drone",
                "pad",
                "sustained",
                "evolving",
                "textural",
                "ambient",
            ],
            "experimental_fx": [
                "experimental",
                "fx",
                "processed",
                "weird",
                "effect",
                "glitch",
            ],
            # === ROCK ===
            "rock_drums": [
                "drums",
                "acoustic",
                "powerful",
                "live",
                "kit",
                "beat",
                "rhythm",
            ],
            "rock_guitar": [
                "guitar",
                "distorted",
                "riff",
                "electric",
                "power",
                "chord",
            ],
            "rock_bass": ["bass", "guitar", "picked", "aggressive", "electric"],
            "rock_synth": ["synth", "rock", "keys", "organ", "keyboard"],
            "rock_fx": ["fx", "guitar", "feedback", "reverb", "effect"],
            # === HIP HOP ===
            "hiphop_beat": [
                "beat",
                "drums",
                "boom",
                "bap",
                "trap",
                "rhythm",
                "kick",
                "808",
            ],
            "hiphop_bass": ["bass", "808", "sub", "deep", "heavy"],
            "hiphop_melody": ["melody", "sample", "piano", "keys", "vocal", "lead"],
            "hiphop_fx": ["fx", "scratch", "vocal", "effect", "chop"],
            # === JUNGLE/DNB ===
            "jungle_break": ["break", "breakbeat", "amen", "drum", "jungle", "chop"],
            "jungle_bass": ["bass", "reese", "sub", "wobble", "deep"],
            "jungle_pad": ["pad", "atmospheric", "ambient", "space", "string"],
            "jungle_fx": ["fx", "siren", "effect", "vocal", "sample"],
            "dnb_percussion": ["percussion", "snare", "hi-hat", "roll", "fill"],
            "dnb_bass": ["bass", "neuro", "wobble", "modulated", "rolling"],
            "dnb_synth": ["synth", "lead", "stab", "chord", "arp"],
            "dnb_fx": ["fx", "impact", "sweep", "riser", "effect"],
            # === DUB ===
            "dub_riddim": ["riddim", "beat", "one", "drop", "steppers", "rhythm"],
            "dub_bass": ["bass", "sub", "deep", "reggae", "wobble"],
            "dub_melody": ["melody", "organ", "piano", "guitar", "lead"],
            "dub_fx": ["fx", "echo", "delay", "siren", "effect"],
            "dub_skanks": ["skanks", "guitar", "chord", "upstroke", "reggae"],
            "dub_horns": ["horns", "brass", "trumpet", "trombone", "section"],
            # === DEEP HOUSE ===
            "house_kick": ["kick", "four", "on", "floor", "thump", "deep"],
            "house_bass": ["bass", "deep", "warm", "analog", "sub"],
            "house_chord": ["chord", "piano", "keys", "stab", "progression"],
            "house_pad": ["pad", "string", "atmospheric", "warm", "lush"],
            "house_percussion": ["percussion", "conga", "bongo", "latin", "organic"],
            "house_fx": ["fx", "vocal", "sweep", "effect", "atmosphere"],
            # === CLASSICAL ===
            "classical_strings": ["strings", "violin", "viola", "cello", "orchestral"],
            "classical_piano": ["piano", "grand", "keys", "classical"],
            "classical_brass": ["brass", "trumpet", "horn", "trombone", "orchestral"],
            "classical_percussion": ["percussion", "timpani", "orchestral", "drums"],
            "classical_choir": ["choir", "vocal", "voices", "choral", "ensemble"],
            "classical_harp": ["harp", "strings", "plucked", "classical"],
            # === AMBIENT ===
            "ambient_pad": ["pad", "atmospheric", "warm", "lush", "evolving"],
            "ambient_texture": [
                "texture",
                "granular",
                "processed",
                "evolving",
                "soundscape",
            ],
            "ambient_drone": ["drone", "sustained", "continuous", "tonal", "deep"],
            "ambient_bells": ["bells", "chimes", "metallic", "resonant", "shimmer"],
            "ambient_drums": ["drums", "soft", "organic", "processed", "rhythm"],
            "ambient_bass": ["bass", "sub", "warm", "deep", "sustained"],
            "field_recording": [
                "field",
                "recording",
                "nature",
                "environment",
                "organic",
            ],
            # === TRIP HOP ===
            "triphop_beat": ["beat", "broken", "downtempo", "drums", "vinyl"],
            "triphop_bass": ["bass", "deep", "heavy", "warm", "analog"],
            "triphop_melody": ["melody", "sample", "piano", "string", "cinematic"],
            "triphop_fx": ["fx", "scratch", "vinyl", "effect", "atmosphere"],
            "triphop_pad": ["pad", "string", "atmospheric", "melancholic", "dark"],
            "triphop_scratch": ["scratch", "vinyl", "turntable", "effect", "hip"],
            # === MOTS-CLÉS GÉNÉRIQUES ===
            "kick": ["kick", "drum", "beat", "thump", "four"],
            "bass": ["bass", "sub", "low", "deep", "bottom"],
            "synth": ["synth", "synthesizer", "keys", "lead", "melody"],
            "percussion": ["percussion", "perc", "hi-hat", "snare", "clap"],
            "pad": ["pad", "string", "atmosphere", "ambient", "wash"],
            "fx": ["fx", "effect", "sweep", "noise", "transition"],
            "drums": ["drums", "kit", "beat", "rhythm", "percussion"],
            "melody": ["melody", "lead", "tune", "melodic", "theme"],
            "rhythm": ["rhythm", "beat", "groove", "pattern", "tempo"],
        }

        # Scorer chaque type disponible
        type_scores = {}
        for sample_type in available_types:
            score = 0

            # Chercher les mots-clés qui correspondent à ce type
            for keyword in cleaned_keywords:
                keyword_lower = keyword.lower()

                # Score DIRECT si le mot-clé est dans le nom du type (score élevé)
                if keyword_lower in sample_type.lower():
                    score += 10

                # Score via mapping spécifique
                if sample_type in keyword_to_type:
                    type_keywords = keyword_to_type[sample_type]
                    for type_keyword in type_keywords:
                        if (
                            type_keyword in keyword_lower
                            or keyword_lower in type_keyword
                        ):
                            score += 5

                # Score générique basé sur les composants du nom du type
                type_parts = sample_type.lower().split("_")
                for part in type_parts:
                    if part in keyword_lower or keyword_lower in part:
                        score += 3

                # Bonus pour correspondances exactes importantes
                exact_matches = {
                    "kick": ["kick", "drum", "beat"],
                    "bass": ["bass", "sub"],
                    "synth": ["synth", "lead", "melody"],
                    "percussion": ["percussion", "perc", "hi-hat", "hihat"],
                    "pad": ["pad", "atmosphere"],
                    "fx": ["fx", "effect"],
                    "break": ["break", "breakbeat", "amen"],
                    "riddim": ["riddim", "rhythm"],
                    "chord": ["chord", "progression"],
                    "drone": ["drone", "sustained"],
                }

                for type_word, match_words in exact_matches.items():
                    if type_word in sample_type.lower():
                        if any(match in keyword_lower for match in match_words):
                            score += 8

            type_scores[sample_type] = score

        # Choisir le type avec le meilleur score
        if type_scores:
            best_type = max(type_scores, key=type_scores.get)
            best_score = type_scores[best_type]

            print(f"   Scores: {type_scores}")
            print(f"   Choix final: {best_type} (score: {best_score})")

            # Si le score est assez bon, utiliser ce choix
            if best_score >= 3:  # Seuil minimal pour être confiant
                return best_type

        # Fallback : premier type de la liste
        default_type = available_types[0] if available_types else f"{genre}_kick"
        print(f"   Fallback (score trop faible): {default_type}")
        return default_type

    @classmethod
    def detect_and_adapt(
        cls,
        user_prompt,
        llm_keywords=None,
        default_genre="techno",
        tempo=126,
        key="C minor",
        intensity=5,
        llm_sample_type=None,
        vst_style_params=None,
    ):
        """
        Version optimisée avec nettoyage intelligent des mots-clés ET sélection de sample type

        Args:
            user_prompt: Prompt de l'utilisateur
            llm_keywords: Mots-clés du LLM
            default_genre: Genre par défaut
            tempo: Tempo en BPM
            key: Tonalité
            intensity: Intensité 1-10
            llm_sample_type: Type suggéré par le LLM ("auto_detect" = adaptation)
            vst_style_params: Paramètres VST avec sample_types disponibles
        """
        # Nettoyer et fusionner les mots-clés
        if llm_keywords:
            cleaned_keywords = KeywordCleaner.clean_and_merge(user_prompt, llm_keywords)
            combined_text = " ".join(cleaned_keywords)
        else:
            cleaned_keywords = user_prompt.split() if user_prompt else []
            combined_text = user_prompt or ""

        if not combined_text:
            # Pas de prompt custom, utiliser les défauts
            return {
                "genre": default_genre,
                "sample_type": cls.GENRE_MAPPING.get(default_genre, {}).get(
                    "sample_types", ["techno_kick"]
                )[0],
                "musicgen_prompt": f"A {default_genre} track at {tempo} BPM, {key}",
                "intensity": intensity,
                "cleaned_keywords": [],
            }

        combined_text_lower = combined_text.lower()
        detected_genre = None

        # Détecter le genre avec le texte nettoyé
        for genre, config in cls.GENRE_MAPPING.items():
            if any(keyword in combined_text_lower for keyword in config["keywords"]):
                detected_genre = genre
                break

        # Auto-detect si demandé par le LLM
        if llm_sample_type == "auto_detect" and not detected_genre:
            print(f"🔍 Auto-detect activé, analyse fine...")
            # Logique d'analyse fine (comme avant)
            if any(
                word in combined_text_lower
                for word in ["kick", "drum", "beat", "rhythm"]
            ):
                if any(
                    word in combined_text_lower
                    for word in ["fast", "174", "170", "180", "dnb", "jungle"]
                ):
                    detected_genre = "drum_and_bass"
                elif any(
                    word in combined_text_lower
                    for word in ["slow", "70", "80", "dub", "reggae"]
                ):
                    detected_genre = "dub"
                else:
                    detected_genre = (
                        default_genre  # Garder le genre par défaut si ambigu
                    )
            elif any(
                word in combined_text_lower
                for word in ["pad", "atmosphere", "space", "chill", "ambient"]
            ):
                detected_genre = "ambient"
            elif any(
                word in combined_text_lower
                for word in ["guitar", "distort", "heavy", "rock"]
            ):
                detected_genre = "rock"

        # Si pas de genre détecté, utiliser le genre par défaut
        if not detected_genre:
            detected_genre = default_genre
            print(
                f"🎯 Aucun genre spécifique détecté, utilisation du genre par défaut: {detected_genre}"
            )

        # Choisir le sample type intelligemment
        adapted_sample_type = cls.choose_sample_type_from_keywords(
            detected_genre, cleaned_keywords, vst_style_params
        )

        # Genre détecté - construction optimisée
        config = cls.GENRE_MAPPING.get(detected_genre, {})

        # Construire le prompt optimisé sans redondance
        intensity_words = [
            "very soft",
            "soft",
            "gentle",
            "moderate",
            "medium",
            "energetic",
            "driving",
            "powerful",
            "intense",
            "very intense",
        ]
        intensity_desc = intensity_words[min(intensity - 1, len(intensity_words) - 1)]

        # Template avec intensité
        template = config.get(
            "template", "A {intensity} {genre} track at {tempo} BPM, {key}"
        )
        template_with_intensity = template.replace("{intensity}", intensity_desc)

        # Construire le prompt final optimisé
        optimized_prompt = KeywordCleaner.build_optimized_prompt(
            template_with_intensity, tempo, key, cleaned_keywords
        )

        print(f"🎯 Détection finale:")
        print(f"   Genre: {detected_genre}")
        print(f"   Sample type: {adapted_sample_type}")
        print(f"   Mots-clés nettoyés: {cleaned_keywords}")
        print(f"   Prompt optimisé: {optimized_prompt}")

        return {
            "genre": detected_genre,
            "sample_type": adapted_sample_type,
            "musicgen_prompt": optimized_prompt,
            "intensity": config.get("default_intensity", intensity),
            "cleaned_keywords": cleaned_keywords,
        }
