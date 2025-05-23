import mido
import threading


class MidiClockManager:
    """Gestionnaire global du MIDI Clock depuis Bitwig avec mido"""

    def __init__(self, port_name: str = "loopMIDI Port"):
        self.port_name = port_name
        self.midi_input = None
        self.is_running = False
        self.listeners = []  # AudioLayers qui écoutent
        self.thread = None

        # État du transport
        self.is_playing = False
        self.clock_count = 0
        self.beat_count = 0
        self.current_measure = 1
        self.current_beat_in_measure = 1

    def add_listener(self, audio_layer):
        """Ajoute un AudioLayer à la sync"""
        if audio_layer not in self.listeners:
            self.listeners.append(audio_layer)
            print(f"🎼 Layer '{audio_layer.layer_id}' ajouté à la sync MIDI")

    def remove_listener(self, audio_layer):
        """Retire un AudioLayer de la sync"""
        if audio_layer in self.listeners:
            self.listeners.remove(audio_layer)
            print(f"🚫 Layer '{audio_layer.layer_id}' retiré de la sync MIDI")

    def start(self):
        """Démarre l'écoute MIDI Clock"""
        if self.is_running:
            return

        # Trouver le port loopMIDI
        available_ports = mido.get_input_names()
        print(f"🎵 Ports MIDI disponibles: {available_ports}")

        target_port = None

        # Chercher loopMIDI Port d'abord
        for port in available_ports:
            if self.port_name.lower() in port.lower():
                target_port = port
                break

        # Sinon chercher d'autres ports connus
        if not target_port:
            for port in available_ports:
                if any(
                    keyword in port.lower()
                    for keyword in ["loopmidi", "bitwig", "virtual"]
                ):
                    target_port = port
                    break

        # En dernier recours, prendre le premier port
        if not target_port and available_ports:
            target_port = available_ports[0]

        if not target_port:
            print(f"❌ Aucun port MIDI trouvé")
            return

        try:
            self.midi_input = mido.open_input(target_port)
            self.is_running = True

            print(f"🎵 MIDI Clock sync démarré sur: {target_port}")

            # Thread d'écoute
            self.thread = threading.Thread(target=self._listen_loop, daemon=True)
            self.thread.start()

        except Exception as e:
            print(f"❌ Erreur MIDI: {e}")

    def stop(self):
        """Arrête l'écoute MIDI"""
        self.is_running = False
        if self.midi_input:
            self.midi_input.close()
        print("⏹️ MIDI Clock sync arrêté")

    def _listen_loop(self):
        """Boucle d'écoute des events MIDI"""
        try:
            for msg in self.midi_input:
                if not self.is_running:
                    break

                if msg.type == "start":
                    print("🟢 START - Bitwig démarre")
                    self._reset_counters()
                    self.is_playing = True
                    self._notify_listeners("start")

                elif msg.type == "stop":
                    print("🔴 STOP - Bitwig s'arrête")
                    self.is_playing = False
                    self._notify_listeners("stop")

                elif msg.type == "continue":
                    print("⏯️ CONTINUE - Bitwig reprend")
                    self.is_playing = True
                    self._notify_listeners("continue")

                elif msg.type == "clock" and self.is_playing:
                    self.clock_count += 1

                    # 24 clocks = 1 beat
                    if self.clock_count % 24 == 0:
                        self.beat_count += 1
                        self.current_measure = ((self.beat_count - 1) // 4) + 1
                        self.current_beat_in_measure = ((self.beat_count - 1) % 4) + 1

                        # Notifier sur le beat 1 de chaque mesure
                        if self.current_beat_in_measure == 1:
                            self._notify_listeners(
                                "measure_start", self.current_measure
                            )

        except Exception as e:
            print(f"❌ Erreur dans la boucle MIDI: {e}")

    def _reset_counters(self):
        """Reset des compteurs lors du start"""
        self.clock_count = 0
        self.beat_count = 0
        self.current_measure = 1
        self.current_beat_in_measure = 1

    def _notify_listeners(self, event_type: str, measure: int = None):
        """Notifie tous les AudioLayers"""
        for listener in self.listeners:
            if hasattr(listener, "on_midi_event"):
                listener.on_midi_event(event_type, measure)

    def debug_status(self):
        """Affiche l'état actuel pour debug"""
        print(f"🔍 MIDI Manager Status:")
        print(f"  Running: {self.is_running}")
        print(f"  Playing: {self.is_playing}")
        print(f"  Current measure: {self.current_measure}")
        print(f"  Beat in measure: {self.current_beat_in_measure}")
        print(f"  Total beats: {self.beat_count}")
        print(f"  Listeners: {len(self.listeners)}")
