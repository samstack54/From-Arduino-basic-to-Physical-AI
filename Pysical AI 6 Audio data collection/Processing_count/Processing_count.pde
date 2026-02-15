int count  = 10 ;

void setup()
{
  size(500, 400) ;
}

void draw()
{
  background(150, 50, 50) ;
  textSize(50) ;
  text("count =", 100, 200) ;
  text(count, 300, 200) ;
  count++ ;
}
