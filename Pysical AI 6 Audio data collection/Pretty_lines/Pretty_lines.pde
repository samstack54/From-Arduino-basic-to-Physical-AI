// Draw lines

void setup() {
size(640,480);
background(0);
smooth();
}

void draw() {
  stroke(random(255),random(255),random(255));
  line(random(width),random(height),width,width);
}
