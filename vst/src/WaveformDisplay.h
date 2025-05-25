// WaveformDisplay.h - Version améliorée
#pragma once
#include "JuceHeader.h"

class WaveformDisplay : public juce::Component, public juce::Timer
{
public:
    WaveformDisplay()
    {
        setSize(400, 80);
        // Ne PAS démarrer le timer ici - seulement quand nécessaire
    }

    ~WaveformDisplay()
    {
        stopTimer(); // Sécurité: s'assurer que le timer s'arrête
    }

    void setAudioData(const juce::AudioBuffer<float> &audioBuffer, double sampleRate)
    {
        this->audioBuffer = audioBuffer;
        this->sampleRate = sampleRate;

        // Reset zoom au chargement
        zoomFactor = 1.0;
        viewStartTime = 0.0;

        generateThumbnail();
        repaint();
    }

    void setLoopPoints(double startTime, double endTime)
    {
        if (!loopPointsLocked)
        {
            loopStart = startTime;
            loopEnd = endTime;
            repaint();
        }
    }

    void lockLoopPoints(bool locked)
    {
        loopPointsLocked = locked;
        repaint();
    }

    // NOUVEAU : Mettre à jour la position de lecture
    void setPlaybackPosition(double timeInSeconds, bool isPlaying)
    {
        playbackPosition = timeInSeconds;
        isCurrentlyPlaying = isPlaying;

        // Auto-scroll pour suivre la tête de lecture si on est zoomé
        if (isPlaying && zoomFactor > 1.0)
        {
            double viewDuration = getTotalDuration() / zoomFactor;
            double viewEndTime = viewStartTime + viewDuration;

            // Si la tête de lecture sort de la vue, centrer dessus
            if (playbackPosition < viewStartTime || playbackPosition > viewEndTime)
            {
                viewStartTime = juce::jlimit(0.0, getTotalDuration() - viewDuration,
                                             playbackPosition - viewDuration * 0.5);
                generateThumbnail();
            }
        }

        repaint(); // Redessiner pour mettre à jour la tête de lecture
    }

    std::function<void(double, double)> onLoopPointsChanged;

    void paint(juce::Graphics &g) override
    {
        auto bounds = getLocalBounds();

        // Background
        g.setColour(juce::Colours::black);
        g.fillRect(bounds);

        if (thumbnail.empty())
        {
            g.setColour(juce::Colours::grey);
            g.setFont(12.0f);
            g.drawText("No audio data", bounds.reduced(5).removeFromTop(20), juce::Justification::centred);

            g.setColour(juce::Colours::lightgrey);
            g.setFont(10.0f);
            g.drawText("Ctrl+Wheel: Zoom | Wheel: Scroll | Right-click: Lock/Unlock",
                       bounds.reduced(5).removeFromBottom(15), juce::Justification::centred);
            return;
        }

        // Dessiner waveform avec zoom
        g.setColour(juce::Colours::lightblue);
        drawWaveform(g);

        // Dessiner markers de loop
        drawLoopMarkers(g);

        // NOUVEAU : Dessiner la tête de lecture - APRÈS les loop markers
        drawPlaybackHead(g);

        // Indicateur de zoom
        if (zoomFactor > 1.0)
        {
            g.setColour(juce::Colours::yellow);
            g.setFont(10.0f);
            g.drawText("Zoom: " + juce::String(zoomFactor, 1) + "x", 5, 5, 60, 15, juce::Justification::left);
        }

        // Indicateur verrouillage
        if (loopPointsLocked)
        {
            g.setColour(juce::Colours::red);
            g.setFont(10.0f);
            g.drawText("LOCKED", getWidth() - 60, 5, 55, 15, juce::Justification::right);
        }
    }

    void timerCallback() override
    {
        // Rafraîchir uniquement si on est en cours de lecture
        if (isCurrentlyPlaying)
        {
            repaint();
        }
    }

    void mouseDown(const juce::MouseEvent &e) override
    {
        if (e.mods.isRightButtonDown())
        {
            // Clic droit = verrouiller/déverrouiller
            loopPointsLocked = !loopPointsLocked;
            repaint();
            return;
        }

        if (loopPointsLocked)
        {
            return; // Pas d'interaction si verrouillé
        }

        float startX = timeToX(loopStart);
        float endX = timeToX(loopEnd);

        if (std::abs(e.x - startX) < 10)
        {
            draggingStart = true;
        }
        else if (std::abs(e.x - endX) < 10)
        {
            draggingEnd = true;
        }
    }

    void mouseDrag(const juce::MouseEvent &e) override
    {
        if (loopPointsLocked)
            return;

        if (draggingStart)
        {
            double newStart = xToTime(e.x);
            loopStart = juce::jlimit(getViewStartTime(), loopEnd - 0.1, newStart);
            repaint();

            if (onLoopPointsChanged)
            {
                onLoopPointsChanged(loopStart, loopEnd);
            }
        }
        else if (draggingEnd)
        {
            double newEnd = xToTime(e.x);
            loopEnd = juce::jlimit(loopStart + 0.1, getViewEndTime(), newEnd);
            repaint();

            if (onLoopPointsChanged)
            {
                onLoopPointsChanged(loopStart, loopEnd);
            }
        }
    }

    void mouseUp(const juce::MouseEvent &e) override
    {
        draggingStart = false;
        draggingEnd = false;
    }

    void mouseWheelMove(const juce::MouseEvent &e, const juce::MouseWheelDetails &wheel) override
    {
        if (e.mods.isCtrlDown())
        {
            // Ctrl + molette = zoom horizontal
            double mouseTime = xToTime(e.x);

            if (wheel.deltaY > 0)
            {
                zoomFactor = juce::jlimit(1.0, 10.0, zoomFactor * 1.2);
            }
            else
            {
                zoomFactor = juce::jlimit(1.0, 10.0, zoomFactor / 1.2);
            }

            // Ajuster viewStartTime pour garder la souris au même endroit
            double newViewDuration = getTotalDuration() / zoomFactor;
            viewStartTime = mouseTime - (e.x / (double)getWidth()) * newViewDuration;
            viewStartTime = juce::jlimit(0.0, getTotalDuration() - newViewDuration, viewStartTime);

            generateThumbnail();
            repaint();
        }
        else if (zoomFactor > 1.0)
        {
            // Molette seule = scroll horizontal quand zoomé
            double viewDuration = getTotalDuration() / zoomFactor;
            double scrollAmount = wheel.deltaY * viewDuration * 0.1;

            viewStartTime = juce::jlimit(0.0, getTotalDuration() - viewDuration,
                                         viewStartTime - scrollAmount);

            generateThumbnail();
            repaint();
        }
    }

private:
    juce::AudioBuffer<float> audioBuffer;
    double sampleRate = 44100.0;
    std::vector<float> thumbnail;

    double loopStart = 0.0;
    double loopEnd = 4.0;
    bool loopPointsLocked = false;

    bool draggingStart = false;
    bool draggingEnd = false;

    // Variables de zoom
    double zoomFactor = 1.0;    // 1.0 = vue complète, >1.0 = zoomé
    double viewStartTime = 0.0; // Début de la vue actuelle

    // NOUVEAU : Variables de tête de lecture
    double playbackPosition = 0.0; // Position actuelle en secondes
    bool isCurrentlyPlaying = false;

    void generateThumbnail()
    {
        thumbnail.clear();

        if (audioBuffer.getNumSamples() == 0)
            return;

        double viewDuration = getTotalDuration() / zoomFactor;
        double viewEndTime = juce::jlimit(viewStartTime, getTotalDuration(),
                                          viewStartTime + viewDuration);

        int startSample = (int)(viewStartTime * sampleRate);
        int endSample = (int)(viewEndTime * sampleRate);
        int viewSamples = endSample - startSample;

        if (viewSamples <= 0)
            return;

        // HAUTE RÉSOLUTION : Plus de points que de pixels pour lisser
        int targetPoints = getWidth() * 4; // 4x plus de points que de pixels
        int samplesPerPoint = std::max(1, viewSamples / targetPoints);

        for (int point = 0; point < targetPoints; ++point)
        {
            int sampleStart = startSample + (point * samplesPerPoint);
            int sampleEnd = std::min(sampleStart + samplesPerPoint, audioBuffer.getNumSamples());

            // Calculer RMS (plus musical que peak) + peak pour les transitoires
            float rmsSum = 0.0f;
            float peak = 0.0f;
            int count = 0;

            for (int sample = sampleStart; sample < sampleEnd; ++sample)
            {
                for (int ch = 0; ch < audioBuffer.getNumChannels(); ++ch)
                {
                    float val = audioBuffer.getSample(ch, sample);
                    rmsSum += val * val;
                    peak = std::max(peak, std::abs(val));
                    count++;
                }
            }

            float rms = count > 0 ? std::sqrt(rmsSum / count) : 0.0f;

            // Mélange RMS + peak pour un meilleur rendu visuel
            float finalValue = (rms * 0.7f) + (peak * 0.3f);

            thumbnail.push_back(finalValue);
        }
    }

    void drawWaveform(juce::Graphics &g)
    {
        if (thumbnail.empty())
            return;

        // RENDU LISSÉ avec antialiasing
        g.setColour(juce::Colours::lightblue);

        // Créer un path smooth
        juce::Path waveformPath;
        bool pathStarted = false;

        int thumbnailSize = thumbnail.size();
        float pixelsPerPoint = (float)getWidth() / thumbnailSize;

        for (int i = 0; i < thumbnailSize; ++i)
        {
            float x = i * pixelsPerPoint;
            float amplitude = thumbnail[i];

            // Courbe symétrique (positif et négatif)
            float centerY = getHeight() * 0.5f;
            float waveHeight = amplitude * centerY * 0.8f; // 80% de la hauteur pour éviter les bords

            float topY = centerY - waveHeight;
            float bottomY = centerY + waveHeight;

            if (!pathStarted)
            {
                // Commencer par le centre
                waveformPath.startNewSubPath(x, centerY);
                pathStarted = true;
            }

            // Créer une courbe lissée avec des points de contrôle
            if (i > 0 && i < thumbnailSize - 1)
            {
                // Utiliser quadraticTo pour lisser
                float prevX = (i - 1) * pixelsPerPoint;
                float nextX = (i + 1) * pixelsPerPoint;

                float controlX = (prevX + nextX) * 0.5f;
                waveformPath.quadraticTo(controlX, topY, x, topY);
            }
            else
            {
                waveformPath.lineTo(x, topY);
            }
        }

        // Dessiner la partie haute
        g.strokePath(waveformPath, juce::PathStrokeType(1.5f, juce::PathStrokeType::curved));

        // Créer la partie basse (miroir)
        juce::Path bottomPath;
        pathStarted = false;

        for (int i = 0; i < thumbnailSize; ++i)
        {
            float x = i * pixelsPerPoint;
            float amplitude = thumbnail[i];

            float centerY = getHeight() * 0.5f;
            float waveHeight = amplitude * centerY * 0.8f;
            float bottomY = centerY + waveHeight;

            if (!pathStarted)
            {
                bottomPath.startNewSubPath(x, centerY);
                pathStarted = true;
            }

            if (i > 0 && i < thumbnailSize - 1)
            {
                float prevX = (i - 1) * pixelsPerPoint;
                float nextX = (i + 1) * pixelsPerPoint;
                float controlX = (prevX + nextX) * 0.5f;
                bottomPath.quadraticTo(controlX, bottomY, x, bottomY);
            }
            else
            {
                bottomPath.lineTo(x, bottomY);
            }
        }

        // Dessiner la partie basse
        g.strokePath(bottomPath, juce::PathStrokeType(1.5f, juce::PathStrokeType::curved));

        // Ajouter une ligne centrale subtile
        g.setColour(juce::Colours::lightblue.withAlpha(0.3f));
        g.drawLine(0, getHeight() * 0.5f, getWidth(), getHeight() * 0.5f, 0.5f);
    }

    void drawLoopMarkers(juce::Graphics &g)
    {
        float startX = timeToX(loopStart);
        float endX = timeToX(loopEnd);

        // Zone de loop
        juce::Colour loopColour = loopPointsLocked ? juce::Colours::orange : juce::Colours::green;
        g.setColour(loopColour.withAlpha(0.2f));
        g.fillRect(startX, 0.0f, endX - startX, (float)getHeight());

        // Markers
        g.setColour(loopColour);
        g.drawLine(startX, 0, startX, getHeight(), loopPointsLocked ? 3.0f : 2.0f);
        g.drawLine(endX, 0, endX, getHeight(), loopPointsLocked ? 3.0f : 2.0f);

        // Labels
        g.setColour(juce::Colours::white);
        g.setFont(10.0f);
        g.drawText(juce::String(loopStart, 2) + "s", startX + 2, 2, 50, 15, juce::Justification::left);
        g.drawText(juce::String(loopEnd, 2) + "s", endX - 50, 2, 48, 15, juce::Justification::right);
    }

    // NOUVEAU : Dessiner la tête de lecture
    void drawPlaybackHead(juce::Graphics &g)
    {
        // DEBUG - toujours afficher quelque chose pour tester
        if (playbackPosition > 0.0 || isCurrentlyPlaying)
        {
            DBG("=== DRAWING PLAYBACK HEAD ===");
            DBG("  playbackPosition: " + juce::String(playbackPosition, 3) + "s");
            DBG("  isCurrentlyPlaying: " + juce::String(isCurrentlyPlaying ? "YES" : "NO"));
            DBG("  getTotalDuration: " + juce::String(getTotalDuration(), 3) + "s");
            DBG("  viewStartTime: " + juce::String(viewStartTime, 3) + "s");
            DBG("  zoomFactor: " + juce::String(zoomFactor, 2));
        }

        if (!isCurrentlyPlaying && playbackPosition <= 0.0)
        {
            return;
        }

        float headX = timeToX(playbackPosition);

        DBG("  timeToX result: " + juce::String(headX) + " (component width: " + juce::String(getWidth()) + ")");

        // TOUJOURS dessiner quelque chose de visible pour debug
        if (isCurrentlyPlaying)
        {
            // Forcer une position visible pour debug
            float debugX = juce::jlimit(10.0f, (float)getWidth() - 10.0f, headX);

            // Ligne de la tête de lecture - TRÈS VISIBLE
            g.setColour(juce::Colours::red);
            g.drawLine(debugX, 0, debugX, getHeight(), 4.0f); // Très épaisse

            // Triangle en haut - GROS
            juce::Path triangle;
            triangle.addTriangle(debugX - 8, 0, debugX + 8, 0, debugX, 16);
            g.setColour(juce::Colours::yellow);
            g.fillPath(triangle);

            // Triangle en bas - GROS
            triangle.clear();
            triangle.addTriangle(debugX - 8, getHeight(), debugX + 8, getHeight(), debugX, getHeight() - 16);
            g.fillPath(triangle);

            // Position en temps - GROS TEXTE
            g.setColour(juce::Colours::white);
            g.setFont(14.0f);
            g.drawText(juce::String(playbackPosition, 2) + "s", debugX - 40, getHeight() / 2 - 10, 80, 20,
                       juce::Justification::centred);

            // Aussi dessiner à la position calculée réelle si différente
            if (std::abs(debugX - headX) > 5.0f)
            {
                g.setColour(juce::Colours::cyan);
                g.drawLine(headX, 0, headX, getHeight(), 2.0f);
                g.drawText("REAL", headX - 20, 5, 40, 15, juce::Justification::centred);
            }

            DBG("  Drew playback head at debugX=" + juce::String(debugX) + ", realX=" + juce::String(headX));
        }
    }

    float timeToX(double time)
    {
        double viewDuration = getTotalDuration() / zoomFactor;
        double relativeTime = time - viewStartTime;
        float result = juce::jmap(relativeTime, 0.0, viewDuration, 0.0, (double)getWidth());

        // DEBUG pour timeToX
        DBG("timeToX: time=" + juce::String(time, 3) +
            ", viewDuration=" + juce::String(viewDuration, 3) +
            ", relativeTime=" + juce::String(relativeTime, 3) +
            ", result=" + juce::String(result, 1));

        return result;
    }

    double xToTime(float x)
    {
        double viewDuration = getTotalDuration() / zoomFactor;
        double relativeTime = juce::jmap((double)x, 0.0, (double)getWidth(), 0.0, viewDuration);
        return viewStartTime + relativeTime;
    }

    double getTotalDuration() const
    {
        return audioBuffer.getNumSamples() / sampleRate;
    }

    double getViewStartTime() const
    {
        return viewStartTime;
    }

    double getViewEndTime() const
    {
        return juce::jlimit(viewStartTime, getTotalDuration(),
                            viewStartTime + (getTotalDuration() / zoomFactor));
    }
};