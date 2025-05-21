import os
import time


def cleanup_output_directory(directory_path, max_age_minutes=60):
    """Nettoie les fichiers temporaires du répertoire de sortie."""
    if not os.path.exists(directory_path):
        os.makedirs(directory_path, exist_ok=True)
        print(f"Répertoire créé: {directory_path}")
        return

    print(f"Nettoyage du répertoire de sortie: {directory_path}")
    try:
        now = time.time()
        count = 0
        for filename in os.listdir(directory_path):
            file_path = os.path.join(directory_path, filename)
            if os.path.isfile(file_path):
                # Vérifier l'âge du fichier
                file_age_minutes = (now - os.path.getmtime(file_path)) / 60.0
                if file_age_minutes > max_age_minutes:
                    try:
                        os.remove(file_path)
                        count += 1
                    except (PermissionError, OSError) as e:
                        print(f"Erreur lors de la suppression de {file_path}: {e}")

        if count > 0:
            print(f"Nettoyage: {count} fichiers temporaires supprimés du répertoire.")
        else:
            print("Aucun fichier à nettoyer.")
    except Exception as e:
        print(f"Erreur lors du nettoyage du répertoire: {e}")
