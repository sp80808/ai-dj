#pragma once
#include "JuceHeader.h"

class WaveformDisplay : public juce::Component
{
public:
    WaveformDisplay()
    {
        setSize(400, 80);

        zoomFactor = 1.0;
        viewStartTime = 0.0;
        sampleRate = 48000.0;
    }

    ~WaveformDisplay()
    {
    }

    void setSampleBpm(float bpm)
    {
        sampleBpm = bpm;
        calculateStretchRatio();
        repaint();
    }

    void setOriginalBpm(float bpm)
    {
        originalBpm = bpm;
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

    void calculateStretchRatio()
    {
        if (originalBpm > 0.0f && sampleBpm > 0.0f)
        {
            stretchRatio = sampleBpm / originalBpm;
        }
        else
        {
            stretchRatio = 1.0f;
        }
    }

    void setPlaybackPosition(double timeInSeconds, bool isPlaying)
    {
        // ✅ Ajuster la position selon le stretch ratio
        double adjustedPosition = timeInSeconds;
        if (stretchRatio > 0.0f && stretchRatio != 1.0f)
        {
            // Si la waveform est étirée, la position doit l'être aussi
            adjustedPosition = timeInSeconds / stretchRatio;
        }

        playbackPosition = adjustedPosition;
        isCurrentlyPlaying = isPlaying;

        // Auto-scroll pour suivre la tête de lecture si on est zoomé
        if (isPlaying && zoomFactor > 1.0)
        {
            double viewDuration = getTotalDuration() / zoomFactor;
            double viewEndTime = viewStartTime + viewDuration;

            if (playbackPosition < viewStartTime || playbackPosition > viewEndTime)
            {
                viewStartTime = juce::jlimit(0.0, getTotalDuration() - viewDuration,
                                             playbackPosition - viewDuration * 0.5);
                generateThumbnail();
            }
        }

        repaint();
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

        drawBeatMarkers(g);
        // Dessiner la tête de lecture - APRÈS les loop markers
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

    void mouseDown(const juce::MouseEvent &e) override
    {
        if (e.mods.isRightButtonDown())
        {
            loopPointsLocked = !loopPointsLocked;
            repaint();
            return;
        }

        if (loopPointsLocked)
            return;

        float startX = timeToX(loopStart);
        float endX = timeToX(loopEnd);

        // Zone de tolérance pour grabber les handles
        float tolerance = 15.0f; // pixels

        if (std::abs(e.x - startX) < tolerance)
        {
            draggingStart = true;
        }
        else if (std::abs(e.x - endX) < tolerance)
        {
            draggingEnd = true;
        }
        else
        {
            double clickTime = xToTime(e.x);

            // Déplacer le marker le plus proche
            double distToStart = std::abs(clickTime - loopStart);
            double distToEnd = std::abs(clickTime - loopEnd);

            if (distToStart < distToEnd)
            {
                loopStart = clickTime;
                // S'assurer que start < end
                if (loopStart >= loopEnd)
                {
                    loopStart = loopEnd - getMinLoopDuration();
                }
            }
            else
            {
                loopEnd = clickTime;
                // S'assurer que end > start
                if (loopEnd <= loopStart)
                {
                    loopEnd = loopStart + getMinLoopDuration();
                }
            }

            if (onLoopPointsChanged)
            {
                onLoopPointsChanged(loopStart, loopEnd);
            }
            repaint();
        }
    }

    void mouseDrag(const juce::MouseEvent &e) override
    {
        if (loopPointsLocked || trackBpm <= 0.0f)
            return;

        if (draggingStart)
        {
            double newStart = xToTime(e.x);

            loopStart = juce::jlimit(getViewStartTime(), loopEnd - getMinLoopDuration(), newStart);
            repaint();

            if (onLoopPointsChanged)
            {
                onLoopPointsChanged(loopStart, loopEnd);
            }
        }
        else if (draggingEnd)
        {
            double newEnd = xToTime(e.x);

            loopEnd = juce::jlimit(loopStart + getMinLoopDuration(), getViewEndTime(), newEnd);
            repaint();

            if (onLoopPointsChanged)
            {
                onLoopPointsChanged(loopStart, loopEnd);
            }
        }
    }

    double getMinLoopDuration()
    {
        if (trackBpm <= 0.0f)
            return 1.0; // 1 seconde par défaut

        double beatDuration = 60.0 / trackBpm;
        return beatDuration * 4.0; // Minimum 1 mesure
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
            if (getTotalDuration() <= 0.0)
            {
                return;
            }

            // Ctrl + molette = zoom horizontal
            double mouseTime = xToTime(e.x);
            mouseTime = juce::jlimit(0.0, getTotalDuration(), mouseTime);
            double oldZoomFactor = zoomFactor;

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
            if (newViewDuration <= 0.0)
            {
                zoomFactor = oldZoomFactor;
                return;
            }

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
    double sampleRate = 48000.0;
    std::vector<float> thumbnail;
    double loopStart = 0.0;
    double loopEnd = 4.0;
    bool loopPointsLocked = false;
    float trackBpm = 126.0f;
    float sampleBpm = 126.0f;
    float stretchRatio = 1.0f;
    bool draggingStart = false;
    bool draggingEnd = false;

    float originalBpm = 126.0f;
    float timeStretchRatio = 1.0f;

    // Variables de zoom
    double zoomFactor = 1.0;    // 1.0 = vue complète, >1.0 = zoomé
    double viewStartTime = 0.0; // Début de la vue actuelle

    // Variables de tête de lecture
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
        startSample = juce::jlimit(0, audioBuffer.getNumSamples() - 1, startSample);
        endSample = juce::jlimit(startSample + 1, audioBuffer.getNumSamples(), endSample);

        int viewSamples = endSample - startSample;

        if (viewSamples <= 0)
            return;

        // HAUTE RÉSOLUTION : Plus de points que de pixels pour lisser
        int targetPoints = getWidth() * 4; // 4x plus de points que de pixels
        int samplesPerPoint = std::max(1, viewSamples / targetPoints);
        targetPoints = juce::jlimit(10, 10000, getWidth() * 4); // Entre 10 et 10000 points
        samplesPerPoint = juce::jlimit(1, viewSamples, viewSamples / targetPoints);

        for (int point = 0; point < targetPoints; ++point)
        {
            int sampleStart = startSample + (point * samplesPerPoint);
            int sampleEnd = std::min(sampleStart + samplesPerPoint, audioBuffer.getNumSamples());
            if (sampleStart >= audioBuffer.getNumSamples())
            {
                break;
            }
            // Calculer RMS (plus musical que peak) + peak pour les transitoires
            float rmsSum = 0.0f;
            float peak = 0.0f;
            int count = 0;

            for (int sample = sampleStart; sample < sampleEnd; ++sample)
            {

                if (sample >= audioBuffer.getNumSamples())
                {
                    break;
                }
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
        juce::Colour waveformColor;
        if (timeStretchRatio > 1.1f)
        {
            waveformColor = juce::Colours::orange;
        }
        else if (timeStretchRatio < 0.9f)
        {
            waveformColor = juce::Colours::lightblue;
        }
        else
        {
            waveformColor = juce::Colours::lightgreen;
        }

        g.setColour(waveformColor);

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

        // Markers avec épaisseur selon le lock
        float lineWidth = loopPointsLocked ? 3.0f : 2.0f;
        g.setColour(loopColour);
        g.drawLine(startX, 0, startX, getHeight(), lineWidth);
        g.drawLine(endX, 0, endX, getHeight(), lineWidth);

        if (trackBpm > 0.0f)
        {
            double beatDuration = 60.0 / trackBpm;
            double barDuration = beatDuration * 4.0;

            int startBar = (int)(loopStart / barDuration) + 1;
            int endBar = (int)(loopEnd / barDuration);
            int totalBars = endBar - startBar + 1;

            g.setColour(juce::Colours::white);
            g.setFont(10.0f);

            // Label start : "Bar 1"
            g.drawText("Bar " + juce::String(startBar), startX + 2, 2, 50, 15,
                       juce::Justification::left);

            // Label end : "Bar 4 (4 bars)"
            g.drawText("Bar " + juce::String(endBar) + " (" + juce::String(totalBars) + " bars)",
                       endX - 80, 2, 78, 15, juce::Justification::right);
        }
        else
        {
            // Fallback sans BPM
            g.setColour(juce::Colours::white);
            g.setFont(10.0f);
            g.drawText(juce::String(loopStart, 2) + "s", startX + 2, 2, 50, 15,
                       juce::Justification::left);
            g.drawText(juce::String(loopEnd, 2) + "s", endX - 50, 2, 48, 15,
                       juce::Justification::right);
        }
    }

    // Dessiner la tête de lecture
    void drawPlaybackHead(juce::Graphics &g)
    {
        if (isCurrentlyPlaying && playbackPosition >= 0.0)
        {
            double displayPosition = playbackPosition;
            if (stretchRatio > 0.0f && stretchRatio != 1.0f)
            {
                displayPosition = playbackPosition / stretchRatio;
            }
            float headX = timeToX(displayPosition);

            if (headX >= 0 && headX <= getWidth())
            {
                g.setColour(juce::Colours::red);
                g.drawLine(headX, 0, headX, getHeight(), 4.0f); // Très épaisse

                // Triangle en haut - GROS
                juce::Path triangle;
                triangle.addTriangle(headX - 8, 0, headX + 8, 0, headX, 16);
                g.setColour(juce::Colours::yellow);
                g.fillPath(triangle);

                // Triangle en bas - GROS
                triangle.clear();
                triangle.addTriangle(headX - 8, getHeight(), headX + 8, getHeight(), headX, getHeight() - 16);
                g.fillPath(triangle);

                // Position en temps - GROS TEXTE
                g.setColour(juce::Colours::white);
                g.setFont(14.0f);
                g.drawText(juce::String(playbackPosition, 2) + "s", headX - 40, getHeight() / 2 - 10, 80, 20,
                    juce::Justification::centred);
            }
        }
        
    }

    float timeToX(double time)
    {
        double totalDuration = getTotalDuration();
        if (totalDuration <= 0.0)
            return 0.0f;

        double viewDuration = totalDuration / zoomFactor;
        if (viewDuration <= 0.0)
            return 0.0f;

        double relativeTime = time - viewStartTime;
        float result = juce::jmap(relativeTime, 0.0, viewDuration, 0.0, (double)getWidth());

        return juce::jlimit(0.0f, (float)getWidth(), result);
    }

    void drawBeatMarkers(juce::Graphics &g)
    {
        if (thumbnail.empty() || sampleBpm <= 0.0f)
            return;

        float totalDuration = getTotalDuration();
        float viewDuration = totalDuration / zoomFactor;
        float viewEndTime = getViewEndTime();

        float beatDuration = 60.0f / sampleBpm;                 // ← Plus le BPM est élevé, plus c'est court !
        float barDuration = beatDuration * 4.0f;                // ← Donc les barres se rapprochent !
        // Dessiner les mesures - L'ESPACEMENT CHANGE !
        g.setColour(juce::Colours::white.withAlpha(0.8f));
        for (float time = 0.0f; time <= viewEndTime; time += barDuration)
        {
            if (time >= viewStartTime)
            {
                float x = timeToX(time);
                if (x >= 0 && x <= getWidth())
                {
                    g.drawLine(x, 0, x, getHeight(), 2.0f);

                    // Numéro de mesure
                    int measureNumber = (int)(time / barDuration) + 1;
                    g.setFont(10.0f);
                    g.drawText(juce::String(measureNumber), x + 2, 2, 30, 15,
                               juce::Justification::left);
                }
            }
        }

        // Beats aussi
        if (zoomFactor > 2.0)
        {
            g.setColour(juce::Colours::white.withAlpha(0.4f));
            for (float time = beatDuration; time <= viewEndTime; time += beatDuration)
            {
                if (fmod(time, barDuration) < 0.01f)
                    continue;

                if (time >= viewStartTime)
                {
                    float x = timeToX(time);
                    if (x >= 0 && x <= getWidth())
                    {
                        g.drawLine(x, 0, x, getHeight(), 1.0f);
                    }
                }
            }
        }
    }

    double xToTime(float x)
    {
        double totalDuration = getTotalDuration();
        if (totalDuration <= 0.0)
        {
            return 0.0;
        }

        double viewDuration = totalDuration / zoomFactor;
        if (viewDuration <= 0.0)
        {
            return 0.0;
        }

        double relativeTime = juce::jmap((double)x, 0.0, (double)getWidth(), 0.0, viewDuration);
        double result = viewStartTime + relativeTime;

        return juce::jlimit(0.0, totalDuration, result);
    }

    double getTotalDuration() const
    {
        if (audioBuffer.getNumSamples() == 0 || sampleRate <= 0)
            return 0.0;

        double originalDuration = audioBuffer.getNumSamples() / sampleRate;

        return originalDuration / timeStretchRatio;
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