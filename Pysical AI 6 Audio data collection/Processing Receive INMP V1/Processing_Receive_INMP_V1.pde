import processing.serial.*;
import javax.sound.sampled.*;
import java.io.*;

processing.serial.Serial myPort;

String portName = "/dev/cu.usbmodem21201"; // ← update if needed
String wavFileName = "yes.wav";
float scaleY = 0.02;  // Increase Y scale

int duration = 10;            // seconds
int sampleRate = 16000;       // Hz
int totalSamples = duration * sampleRate;
int[] samples = new int[totalSamples];
int sampleIndex = 0;

void setup() {
  size(800, 400);
  println("Opening port: " + portName);
  myPort = new processing.serial.Serial(this, portName, 2000000);
  myPort.bufferUntil('\n');
  background(255);
  stroke(0);
  println("Listening for " + duration + " seconds...");
}

void draw() {
  background(255);
  stroke(0);
  noFill();
  beginShape();

  for (int i = 0; i < sampleIndex; i++) {
    float x = map(i, 0, totalSamples, 0, width);
    //float y = height / 2 - samples[i] * scaleY / 32768.0;
    float y = height / 2 - samples[i] * scaleY;
    vertex(x, y);
  }

  endShape();
}

void serialEvent(processing.serial.Serial p) {
  if (sampleIndex >= totalSamples) return;

  String inData = p.readStringUntil('\n');
  if (inData == null) return;

  try {
    int val = Integer.parseInt(inData.trim());
    samples[sampleIndex] = val;
    sampleIndex++;
    
    if (sampleIndex >= totalSamples) {
      saveWav(samples, wavFileName, sampleRate);
      println("WAV saved as " + wavFileName);
    }
  } catch (Exception e) {
    println("Invalid data: " + inData);
  }
}

void saveWav(int[] data, String filename, int sampleRate) {
  try {
    byte[] byteBuffer = new byte[data.length * 2];  // 16-bit PCM
    for (int i = 0; i < data.length; i++) {
      int clamped = constrain(data[i], -32768, 32767);
      byteBuffer[2*i] = (byte)(clamped & 0xFF);
      byteBuffer[2*i+1] = (byte)((clamped >> 8) & 0xFF);
    }

    AudioFormat format = new AudioFormat(sampleRate, 16, 1, true, false);
    ByteArrayInputStream bais = new ByteArrayInputStream(byteBuffer);
    AudioInputStream ais = new AudioInputStream(bais, format, data.length);
    File wavFile = new File(sketchPath(filename));
    AudioSystem.write(ais, AudioFileFormat.Type.WAVE, wavFile);
  } catch (Exception e) {
    e.printStackTrace();
  }
}
