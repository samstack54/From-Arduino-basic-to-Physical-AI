// Receive Audio sample from INMP_Collect_V2.ino

import processing.serial.*;
import javax.sound.sampled.*;
import java.io.*;

// ── Serial port — update to match your board ─────────────────
String portName = "/dev/cu.usbmodem211401";  // Mac
// String portName = "COM3";                // PC

// ── Collection settings ───────────────────────────────────────
String label       = "yes";    // 
int sampleRate     = 16000;
int recordMs       = 1000;     // 1 second per sample
int totalSamples   = sampleRate * recordMs / 1000;  // 16000

// ── State ─────────────────────────────────────────────────────
processing.serial.Serial myPort;
int[]   samples       = new int[totalSamples];
int     sampleIndex   = 0;
boolean recording     = false;
boolean waitingStart  = true;
int     fileCount     = 0;
String  statusMsg     = "Waiting for button press on ESP32...";
float   lastRMS       = 0;

// ── Waveform display ──────────────────────────────────────────
float scaleY = 0.015; //0.003;  // adjust if waveform is too tall/short

void setup() {
  size(800, 450);
  textSize(14);

  println("Opening port: " + portName);
  myPort = new processing.serial.Serial(this, portName, 115200);
  myPort.bufferUntil('\n');

  background(30);
  println("Ready. Press button on ESP32 to start recording.");
  println("Label: " + label);
}

void draw() {
  background(30);

  // ── Waveform ───────────────────────────────────────────────
  stroke(0, 200, 100);
  strokeWeight(1);
  noFill();
  beginShape();
  int drawCount = recording ? sampleIndex : (fileCount > 0 ? totalSamples : 0);
  for (int i = 0; i < drawCount; i++) {
    float x = map(i, 0, totalSamples, 0, width);
    float y = height / 2 - samples[i] * scaleY;
    y = constrain(y, 0, height - 80);
    vertex(x, y);
  }
  endShape();

  // ── Centre line ────────────────────────────────────────────
  stroke(60);
  strokeWeight(1);
  line(0, height / 2, width, height / 2);

  // ── Progress bar ───────────────────────────────────────────
  if (recording) {
    float progress = (float)sampleIndex / totalSamples;
    fill(0, 150, 255);
    noStroke();
    rect(0, height - 20, width * progress, 20);
    fill(255);
    text(nf(int(progress * 100), 0) + "%  " +
         nf(sampleIndex, 0) + " / " + totalSamples + " samples",
         10, height - 4);
  }

  // ── Status panel ───────────────────────────────────────────
  noStroke();
  fill(20);
  rect(0, height - 75, width, 55);

  // Recording indicator
  if (recording) {
    fill(255, 50, 50);
    ellipse(20, height - 55, 14, 14);
    fill(255, 80, 80);
    textSize(14);
    text("RECORDING...", 32, height - 48);
  } else {
    fill(0, 200, 100);
    ellipse(20, height - 55, 14, 14);
    fill(180);
    textSize(14);
    text("IDLE — press button on ESP32", 32, height - 48);
  }

  // File info + RMS
  fill(200);
  textSize(12);
  text("Label: " + label +
       "   Files saved: " + fileCount +
       "   Last RMS: " + nf(lastRMS, 0, 1) +
       (lastRMS < 100 ? "  ← LOW (speak louder/sooner)" : "  ← Good"),
       10, height - 28);

  // Status message
  fill(150);
  text(statusMsg, 10, height - 8);

  // ── Title ──────────────────────────────────────────────────
  fill(255);
  textSize(13);
  text("INMP441 Collector  |  " + sampleRate + " Hz  |  " +
       recordMs + " ms  |  collecting: \"" + label + "\"", 10, 20);
}

// ── Serial handler ────────────────────────────────────────────
void serialEvent(processing.serial.Serial p) {
  String inData = p.readStringUntil('\n');
  if (inData == null) return;
  inData = inData.trim();
  if (inData.length() == 0) return;

  // Arduino sends "START" when button is pressed
  if (inData.equals("START")) {
    if (!recording) {
      startRecording();
    }
    return;
  }

  // While recording, parse sample values
  if (recording) {
    try {
      int val = Integer.parseInt(inData);
      if (sampleIndex < totalSamples) {
        samples[sampleIndex++] = val;
      }
      if (sampleIndex >= totalSamples) {
        finishRecording();
      }
    } catch (Exception e) {
      // Ignore non-numeric lines (e.g. debug messages)
    }
  }
}

void startRecording() {
  sampleIndex = 0;
  recording   = true;
  for (int i = 0; i < totalSamples; i++) samples[i] = 0;
  statusMsg = "Recording started...";
  println("Recording started.");
}

void finishRecording() {
  recording = false;

  // Compute RMS
  double sum = 0;
  for (int i = 0; i < totalSamples; i++) sum += (double)samples[i] * samples[i];
  lastRMS = (float)Math.sqrt(sum / totalSamples);

  if (lastRMS < 100) {
    statusMsg = "WARNING: RMS too low (" + nf(lastRMS, 0, 1) + ") — sample discarded. Speak louder/sooner.";
    println(statusMsg);
    // Don't save — reset and wait for next press
    waitingStart = true;
    return;
  }

  // Auto-numbered filename: yes_001.wav, yes_002.wav ...
  fileCount++;
  String filename = label + "_" + nf(fileCount, 3) + ".wav";
  saveWav(samples, filename, sampleRate);

  statusMsg = "Saved: " + filename + "  RMS=" + nf(lastRMS, 0, 1);
  println(statusMsg);
  println("Press button for next sample. (" + fileCount + " saved so far)");
  waitingStart = true;
}

// ── WAV writer ────────────────────────────────────────────────
void saveWav(int[] data, String filename, int sr) {
  try {
    byte[] byteBuffer = new byte[data.length * 2];
    for (int i = 0; i < data.length; i++) {
      int clamped = constrain(data[i], -32768, 32767);
      byteBuffer[2*i]   = (byte)(clamped & 0xFF);
      byteBuffer[2*i+1] = (byte)((clamped >> 8) & 0xFF);
    }
    // 16000 Hz, 16-bit, mono, signed, little-endian — matches ESP32 capture
    AudioFormat format = new AudioFormat(sr, 16, 1, true, false);
    ByteArrayInputStream bais = new ByteArrayInputStream(byteBuffer);
    AudioInputStream ais = new AudioInputStream(bais, format, data.length);
    File wavFile = new File(sketchPath(filename));
    AudioSystem.write(ais, AudioFileFormat.Type.WAVE, wavFile);
    println("WAV saved: " + wavFile.getAbsolutePath());
  } catch (Exception e) {
    e.printStackTrace();
    statusMsg = "ERROR saving WAV: " + e.getMessage();
  }
}

// ── Keyboard shortcuts ────────────────────────────────────────
void keyPressed() {
  // Press 'y' / 'n' / 'm' to quickly switch label
  if (key == 'y') { label = "yes";   fileCount = 0; statusMsg = "Label changed to: yes";   println("Label: yes"); }
  if (key == 'n') { label = "no";    fileCount = 0; statusMsg = "Label changed to: no";    println("Label: no"); }
  if (key == 'm') { label = "noise"; fileCount = 0; statusMsg = "Label changed to: noise"; println("Label: silent"); }
  // Press 'r' to manually reset counter
  if (key == 'r') { fileCount = 0; statusMsg = "File counter reset."; }
}
