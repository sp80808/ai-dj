import time
import argparse
import pygame
from core.dj_system import DJSystem


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
    args = parser.parse_args()

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
