// Receive from ESP32S3Free image collection
import processing.serial.*;
import java.io.*;
import java.util.*;
import java.util.Base64;

// ===== CONFIGURABLE SETTINGS =====
String fileNamePrefix = "img";    // File name prefix (matches ESP32)
String imageCollectionFolder = "image collection"; // Folder name for saving images
processing.serial.Serial myPort;

// =================== Check ===========================
String portName = "/dev/cu.usbmodem21201"; // Update with your ESP32 port
String[] labels = {"clock", "apple", "obect", "ground"};

int selectedLabelIndex = 0;
// UI Layout
int previewWidth = 300;
int previewHeight = 300;
int galleryItemSize = 80;
int buttonWidth = 80;
int buttonHeight = 40;

// UI State
boolean connected = false;
boolean streaming = false;
PImage currentPreview;
ArrayList<PImage> imageGallery = new ArrayList<PImage>();
ArrayList<String> imageNames = new ArrayList<String>();
String currentLabel = "apple";
int selectedGalleryIndex = -1;

// UI Colors
color bgColor = color(30, 30, 30);
color panelColor = color(50, 50, 50);
color buttonColor = color(70, 130, 180);
color buttonHoverColor = color(100, 160, 210);
color saveButtonColor = color(34, 139, 34);
color deleteButtonColor = color(220, 20, 60);
color textColor = color(255);

// Button positions
int saveButtonX, saveButtonY;
int stopButtonX, stopButtonY;
int deleteButtonX, deleteButtonY;
int streamButtonX, streamButtonY;

// Labels
//String[] labels = {"apple", "orange", "banana", "other"};
//int selectedLabelIndex = 0;

void setup() {
  size(1000, 700);
  
  // Create image collection folder
  File folder = new File(sketchPath(imageCollectionFolder));
  if (!folder.exists()) {
    folder.mkdirs();
  }
  
  // Initialize UI positions
  saveButtonX = previewWidth + 20;
  saveButtonY = 50;
  stopButtonX = saveButtonX + buttonWidth + 10;
  stopButtonY = saveButtonY;
  deleteButtonX = saveButtonX;
  deleteButtonY = saveButtonY + buttonHeight + 10;
  streamButtonX = stopButtonX;
  streamButtonY = deleteButtonY;
  
  // Load existing images from folder
  loadImageGallery();
  
  // Connect to ESP32
  println("Available ports:");
  println(processing.serial.Serial.list());
  
  try {
    myPort = new processing.serial.Serial(this, portName, 115200);
    myPort.bufferUntil('\n');
    connected = true;
    println("Connected to ESP32");
    
    // Start streaming automatically
    delay(2000); // Wait for ESP32 to initialize
    startStreaming();
  } catch (Exception e) {
    println("Failed to connect: " + e.getMessage());
  }
}

void draw() {
  background(bgColor);
  
  // Draw main preview area
  drawPreviewArea();
  
  // Draw control buttons
  drawControlButtons();
  
  // Draw label selector
  drawLabelSelector();
  
  // Draw image gallery
  drawImageGallery();
  
  // Draw status info
  drawStatusInfo();
}

void drawPreviewArea() {
  // Preview frame
  fill(panelColor);
  stroke(100);
  rect(10, 10, previewWidth, previewHeight);
  
  // Current image preview
  if (currentPreview != null) {
    // Scale image to fit preview area while maintaining aspect ratio
    float scale = min((float)previewWidth / currentPreview.width, 
                     (float)previewHeight / currentPreview.height);
    int scaledWidth = (int)(currentPreview.width * scale);
    int scaledHeight = (int)(currentPreview.height * scale);
    int x = 10 + (previewWidth - scaledWidth) / 2;
    int y = 10 + (previewHeight - scaledHeight) / 2;
    
    image(currentPreview, x, y, scaledWidth, scaledHeight);
  } else {
    // No image placeholder
    fill(textColor);
    textAlign(CENTER, CENTER);
    textSize(16);
    text("No Image\n" + (streaming ? "Streaming..." : "Not Streaming"), 
         10 + previewWidth/2, 10 + previewHeight/2);
  }
}

void drawControlButtons() {
  textAlign(CENTER, CENTER);
  textSize(12);
  
  // Save button
  fill(mouseOverButton(saveButtonX, saveButtonY) ? color(64, 179, 64) : saveButtonColor);
  rect(saveButtonX, saveButtonY, buttonWidth, buttonHeight);
  fill(textColor);
  text("SAVE", saveButtonX + buttonWidth/2, saveButtonY + buttonHeight/2);
  
  // Stop/Start streaming button
  color streamColor = streaming ? deleteButtonColor : buttonColor;
  fill(mouseOverButton(streamButtonX, streamButtonY) ? 
       (streaming ? color(250, 50, 90) : buttonHoverColor) : streamColor);
  rect(streamButtonX, streamButtonY, buttonWidth, buttonHeight);
  fill(textColor);
  text(streaming ? "STOP" : "START", streamButtonX + buttonWidth/2, streamButtonY + buttonHeight/2);
  
  // Delete button (only show if image is selected in gallery)
  if (selectedGalleryIndex >= 0) {
    fill(mouseOverButton(deleteButtonX, deleteButtonY) ? color(250, 50, 90) : deleteButtonColor);
    rect(deleteButtonX, deleteButtonY, buttonWidth, buttonHeight);
    fill(textColor);
    text("DELETE", deleteButtonX + buttonWidth/2, deleteButtonY + buttonHeight/2);
  }
}

void drawLabelSelector() {
  fill(textColor);
  textAlign(LEFT);
  textSize(14);
  text("Label:", previewWidth + 20, 150);
  
  // Label buttons
  for (int i = 0; i < labels.length; i++) {
    int x = previewWidth + 20 + (i % 2) * 90;
    int y = 160 + (i / 2) * 35;
    
    if (i == selectedLabelIndex) {
      fill(buttonHoverColor);
    } else {
      fill(mouseOverButton(x, y, 80, 30) ? buttonHoverColor : buttonColor);
    }
    rect(x, y, 80, 30);
    
    fill(textColor);
    textAlign(CENTER, CENTER);
    text(labels[i], x + 40, y + 15);
  }
  
  currentLabel = labels[selectedLabelIndex];
}

void drawImageGallery() {
  fill(textColor);
  textAlign(LEFT);
  textSize(14);
  text("Image Gallery (" + imageGallery.size() + " images):", 10, 340);
  
  // Gallery grid
  int cols = (width - 20) / (galleryItemSize + 5);
  int startY = 360;
  
  for (int i = 0; i < imageGallery.size(); i++) {
    int col = i % cols;
    int row = i / cols;
    int x = 10 + col * (galleryItemSize + 5);
    int y = startY + row * (galleryItemSize + 5);
    
    // Highlight selected image
    if (i == selectedGalleryIndex) {
      stroke(color(255, 255, 0));
      strokeWeight(3);
    } else {
      stroke(100);
      strokeWeight(1);
    }
    
    // Draw image thumbnail
    PImage img = imageGallery.get(i);
    if (img != null) {
      image(img, x, y, galleryItemSize, galleryItemSize);
    } else {
      fill(panelColor);
      rect(x, y, galleryItemSize, galleryItemSize);
    }
    
    // Image name
    fill(textColor);
    textAlign(CENTER);
    textSize(8);
    text(imageNames.get(i), x + galleryItemSize/2, y + galleryItemSize + 12);
  }
  
  noStroke();
}

void drawStatusInfo() {
  fill(textColor);
  textAlign(LEFT);
  textSize(12);
  
  int infoY = height - 60;
  text("Status: " + (connected ? "Connected" : "Disconnected"), 10, infoY);
  text("Streaming: " + (streaming ? "ON" : "OFF"), 10, infoY + 15);
  text("Current Label: " + currentLabel, 10, infoY + 30);
  text("Images: " + imageGallery.size(), 200, infoY);
  text("Selected: " + (selectedGalleryIndex >= 0 ? imageNames.get(selectedGalleryIndex) : "None"), 200, infoY + 15);
}

boolean mouseOverButton(int x, int y) {
  return mouseOverButton(x, y, buttonWidth, buttonHeight);
}

boolean mouseOverButton(int x, int y, int w, int h) {
  return mouseX >= x && mouseX <= x + w && mouseY >= y && mouseY <= y + h;
}

void mousePressed() {
  // Control buttons
  if (mouseOverButton(saveButtonX, saveButtonY)) {
    saveCurrentImage();
  } else if (mouseOverButton(streamButtonX, streamButtonY)) {
    toggleStreaming();
  } else if (selectedGalleryIndex >= 0 && mouseOverButton(deleteButtonX, deleteButtonY)) {
    deleteSelectedImage();
  }
  
  // Label selector
  for (int i = 0; i < labels.length; i++) {
    int x = previewWidth + 20 + (i % 2) * 90;
    int y = 160 + (i / 2) * 35;
    if (mouseOverButton(x, y, 80, 30)) {
      selectedLabelIndex = i;
      break;
    }
  }
  
  // Gallery selection
  int cols = (width - 20) / (galleryItemSize + 5);
  int startY = 360;
  
  for (int i = 0; i < imageGallery.size(); i++) {
    int col = i % cols;
    int row = i / cols;
    int x = 10 + col * (galleryItemSize + 5);
    int y = startY + row * (galleryItemSize + 5);
    
    if (mouseX >= x && mouseX <= x + galleryItemSize && 
        mouseY >= y && mouseY <= y + galleryItemSize) {
      selectedGalleryIndex = (selectedGalleryIndex == i) ? -1 : i;
      break;
    }
  }
}

void serialEvent(processing.serial.Serial port) {
  String inString = port.readStringUntil('\n');
  if (inString != null) {
    inString = trim(inString);
    handleSerialMessage(inString);
  }
}

void handleSerialMessage(String message) {
  if (message.startsWith("STREAM_IMAGE:")) {
    // Format: STREAM_IMAGE:size,base64data
    String data = message.substring(13); // Remove "STREAM_IMAGE:"
    int commaIndex = data.indexOf(',');
    if (commaIndex > 0) {
      String base64Data = data.substring(commaIndex + 1);
      currentPreview = decodeBase64Image(base64Data);
    }
  } else if (message.startsWith("IMAGE_SAVED:")) {
    String[] parts = message.substring(12).split(",");
    if (parts.length >= 3) {
      String filename = parts[0];
      String base64Data = parts[2];
      PImage savedImage = decodeBase64Image(base64Data);
      addToGallery(savedImage, filename);
      println("Image saved: " + filename);
    }
  } else if (message.equals("STREAM_STARTED")) {
    streaming = true;
  } else if (message.equals("STREAM_STOPPED")) {
    streaming = false;
  }
}

PImage decodeBase64Image(String base64Data) {
  try {
    byte[] imageBytes = Base64.getDecoder().decode(base64Data);
    // Create temporary file to load image
    File tempFile = File.createTempFile("temp_image", ".jpg");
    FileOutputStream fos = new FileOutputStream(tempFile);
    fos.write(imageBytes);
    fos.close();
    
    PImage img = loadImage(tempFile.getAbsolutePath());
    tempFile.delete();
    return img;
  } catch (Exception e) {
    println("Error decoding image: " + e.getMessage());
    return null;
  }
}

void addToGallery(PImage img, String filename) {
  if (img != null) {
    imageGallery.add(img);
    imageNames.add(filename.substring(filename.lastIndexOf('/') + 1));
    
    // Also save to computer folder
    saveImageToFolder(img, filename);
  }
}

void saveImageToFolder(PImage img, String filename) {
  String cleanFilename = filename.substring(filename.lastIndexOf('/') + 1);
  String fullPath = sketchPath(imageCollectionFolder + "/" + cleanFilename);
  img.save(fullPath);
}

void loadImageGallery() {
  File folder = new File(sketchPath(imageCollectionFolder));
  File[] files = folder.listFiles();
  
  if (files != null) {
    for (File file : files) {
      if (file.getName().toLowerCase().endsWith(".jpg") || 
          file.getName().toLowerCase().endsWith(".jpeg")) {
        PImage img = loadImage(file.getAbsolutePath());
        if (img != null) {
          imageGallery.add(img);
          imageNames.add(file.getName());
        }
      }
    }
  }
}

void startStreaming() {
  if (connected && myPort != null) {
    myPort.write("start_stream\n");
  }
}

void stopStreaming() {
  if (connected && myPort != null) {
    myPort.write("stop_stream\n");
  }
}

void toggleStreaming() {
  if (streaming) {
    stopStreaming();
  } else {
    startStreaming();
  }
}

void saveCurrentImage() {
  if (connected && myPort != null && currentPreview != null) {
    myPort.write("capture " + currentLabel + "\n");
  }
}

void deleteSelectedImage() {
  if (selectedGalleryIndex >= 0 && selectedGalleryIndex < imageGallery.size()) {
    // Remove from gallery
    imageGallery.remove(selectedGalleryIndex);
    String filename = imageNames.remove(selectedGalleryIndex);
    
    // Delete file from computer
    File file = new File(sketchPath(imageCollectionFolder + "/" + filename));
    if (file.exists()) {
      file.delete();
    }
    
    selectedGalleryIndex = -1;
    println("Deleted: " + filename);
  }
}
