import mido
import time


def test_midi_clock():
    """Test pour voir si on reçoit le MIDI clock de Bitwig"""
    print("Ports MIDI disponibles:")
    for port in mido.get_input_names():
        print(f"  - {port}")

    try:
        # Essaie plusieurs noms possibles
        possible_ports = [
            "Microsoft GS Wavetable Synth",
            "MicrosoftGS WinSynth",
            "Microsoft MIDI Mapper",
            "WinSynth",
            "Bitwig_Clock",
        ]

        port = None
        for port_name in possible_ports:
            if port_name in mido.get_input_names():
                port = mido.open_input(port_name)
                print(f"✅ Connecté sur: {port_name}")
                break

        if not port:
            # Prendre le premier port disponible
            available = mido.get_input_names()
            if available:
                port = mido.open_input(available[0])
                print(f"🔄 Connecté sur: {available[0]}")
            else:
                print("❌ Aucun port MIDI trouvé")
                return
        print(f"\n🎵 Écoute MIDI sur: {port.name}")
        print("▶️  Lance la lecture dans Bitwig...")

        clock_count = 0
        start_time = None

        for msg in port:
            if msg.type == "start":
                print("🟢 START reçu de Bitwig")
                clock_count = 0
                start_time = time.time()

            elif msg.type == "stop":
                print("🔴 STOP reçu de Bitwig")

            elif msg.type == "clock":
                clock_count += 1

                # 24 clocks = 1 beat
                if clock_count % 24 == 0:
                    beat = clock_count // 24
                    if start_time:
                        elapsed = time.time() - start_time
                        print(f"🥁 Beat {beat} (temps écoulé: {elapsed:.2f}s)")

            elif msg.type == "continue":
                print("⏯️ CONTINUE reçu de Bitwig")

    except Exception as e:
        print(f"❌ Erreur: {e}")
        print("Vérifie que Bitwig envoie bien sur le bon port MIDI")


if __name__ == "__main__":
    test_midi_clock()
