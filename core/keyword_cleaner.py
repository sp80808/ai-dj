# Dans genre_detector.py - améliorer KeywordCleaner


class KeywordCleaner:
    """Nettoie et optimise les mots-clés pour éviter la redondance"""

    @staticmethod
    def clean_and_merge(user_prompt, llm_keywords):
        """Combine intelligemment en nettoyant mieux"""
        if not user_prompt:
            return llm_keywords

        # Nettoyer le prompt utilisateur
        user_words = []
        for word in user_prompt.split():
            # Remplacer les underscores par des espaces pour des mots plus naturels
            if "_" in word:
                user_words.extend(word.replace("_", " ").split())
            else:
                user_words.append(word)

        # Filtrer les mots trop courts et nettoyer
        user_words = [
            word.lower().strip() for word in user_words if len(word.strip()) > 2
        ]
        llm_words = [
            word.lower().strip() for word in llm_keywords if len(word.strip()) > 2
        ]

        # Supprimer les doublons intelligemment
        seen = set()
        unique_words = []

        # D'abord les mots de l'utilisateur (nettoyés)
        for word in user_words:
            if word not in seen and word not in [
                "loop",
                "track",
                "sound",
            ]:  # Filtrer mots génériques
                unique_words.append(word)
                seen.add(word)

        # Puis les mots-clés du LLM qui ne sont pas déjà présents
        for word in llm_words:
            if word not in seen:
                unique_words.append(word)
                seen.add(word)

        # Limiter à 6 mots max (plus concis)
        final_keywords = unique_words[:6]

        print(f"🧹 Nettoyage mots-clés optimisé:")
        print(f"   User brut: '{user_prompt}' -> {user_words}")
        print(f"   LLM: {llm_words}")
        print(f"   Final: {final_keywords}")

        return final_keywords

    @staticmethod
    def build_optimized_prompt(template, tempo, key, cleaned_keywords):
        """Construit un prompt plus naturel"""
        # Construire le prompt de base
        base_prompt = template.format(tempo=tempo, key=key, intensity="energetic")

        # Identifier les mots déjà dans le template (plus intelligent)
        template_words_lower = set(
            word.lower().strip(",.") for word in base_prompt.split()
        )

        # Filtrer les mots-clés qui ne sont pas déjà dans le template
        additional_keywords = []
        for word in cleaned_keywords:
            # Vérifier aussi les variations (driving/drive, etc.)
            word_variations = [word, word.rstrip("s"), word + "s"]
            if not any(var in template_words_lower for var in word_variations):
                additional_keywords.append(word)

        # Construire le prompt final plus naturel
        if additional_keywords:
            # Joindre avec des virgules propres
            keywords_str = ", ".join(additional_keywords)
            final_prompt = f"{base_prompt}, {keywords_str}"
        else:
            final_prompt = base_prompt

        # Nettoyage final
        final_prompt = final_prompt.replace(" ,", ",").replace(",,", ",")
        final_prompt = " ".join(final_prompt.split())  # Normaliser espaces

        print(f"🎼 Prompt final optimisé: '{final_prompt}'")

        return final_prompt
