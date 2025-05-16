import time
import argparse
import pygame
import os
import shutil
from core.dj_system import DJSystem


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


def main():
    parser = argparse.ArgumentParser(description="DJ-IA System avec Layer Manager")
    parser.add_argument(
        "--model-path",
        type=str,
        default="google/gemma-2b-it",
        help="Chemin ou nom du modèle LLM",
    )
    parser.add_argument(
        "--profile", type=str, default="techno_minimal", help="Profil DJ à utiliser"
    )
    parser.add_argument(
        "--output-dir",
        type=str,
        default="./dj_session_output",
        help="Répertoire de sortie principal pour la session",
    )
    parser.add_argument(
        "--clean",
        action="store_true",
        help="Nettoyer tous les fichiers du répertoire de sortie au démarrage",
    )
    args = parser.parse_args()

    # Nettoyer les répertoires de sortie
    layers_dir = os.path.join(args.output_dir, "layers")
    speech_dir = os.path.join(args.output_dir, "speech")

    # Créer les répertoires s'ils n'existent pas
    os.makedirs(args.output_dir, exist_ok=True)

    if args.clean:
        # Option pour supprimer complètement le contenu des répertoires
        if os.path.exists(layers_dir):
            shutil.rmtree(layers_dir)
        if os.path.exists(speech_dir):
            shutil.rmtree(speech_dir)
        print("Répertoires de sortie entièrement nettoyés.")
    else:
        # Nettoyer uniquement les fichiers plus anciens que 60 minutes
        cleanup_output_directory(layers_dir)
        cleanup_output_directory(speech_dir)

    # Recréer les répertoires après nettoyage complet
    os.makedirs(layers_dir, exist_ok=True)
    os.makedirs(speech_dir, exist_ok=True)

    dj_system_instance = None  # Pour le finally
    try:
        dj_system_instance = DJSystem(args)
        dj_system_instance.start_session()
        print("DJ-IA en cours d'exécution. Appuyez sur Ctrl+C pour arrêter.")
        while dj_system_instance.session_running:
            if (
                hasattr(dj_system_instance, "dj_thread")
                and not dj_system_instance.dj_thread.is_alive()
            ):
                print(
                    "ALERTE: Le thread principal du DJ s'est terminé de manière inattendue!"
                )
                dj_system_instance.session_running = (
                    False  # Forcer l'arrêt de la boucle main
                )
                break
            time.sleep(1)
    except KeyboardInterrupt:
        print("\nArrêt manuel demandé par l'utilisateur...")
    except Exception as e:
        print(f"Une erreur non gérée est survenue dans main(): {e}")
        import traceback

        traceback.print_exc()
    finally:
        if dj_system_instance:
            print("Nettoyage final de la session DJ...")
            dj_system_instance.stop_session()
        elif pygame.mixer.get_init():  # Si DJSystem n'a pas été créé mais pygame oui
            pygame.mixer.quit()
    print("Programme DJ-IA terminé.")


if __name__ == "__main__":
    main()
