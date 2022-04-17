#include <Arduino.h>
#include <SD.h>
#include <SPI.h>
#include "registers.h"

#define TFTWIDTH 320
#define TFTHEIGHT 480
#define TFTLCD_DELAY 0xFF
uint32_t read32(File f);
uint16_t read16(File f);
void write8inline(uint8_t d);
void bmpDraw(char *filename, int x, int y);
void setAddrWindow(int x1, int y1, int x2, int y2);
void pushColors(uint16_t *data, uint8_t len, boolean first);
uint16_t color565(uint8_t r, uint8_t g, uint8_t b);
uint8_t read8inline();
void write8inline(uint8_t d);
void writeRegister24(uint8_t r, uint32_t d);
uint16_t readID(void);
uint32_t readReg(uint8_t r);

static const uint8_t HX8357D_regValues[] PROGMEM = {
    HX8357_SWRESET,
    0,
    HX8357D_SETC,
    3,
    0xFF,
    0x83,
    0x57,
    TFTLCD_DELAY,
    250,
    HX8357_SETRGB,
    4,
    0x00,
    0x00,
    0x06,
    0x06,
    HX8357D_SETCOM,
    1,
    0x25, // -1.52V
    HX8357_SETOSC,
    1,
    0x68, // Normal mode 70Hz, Idle mode 55 Hz
    HX8357_SETPANEL,
    1,
    0x05, // BGR, Gate direction swapped
    HX8357_SETPWR1,
    6,
    0x00,
    0x15,
    0x1C,
    0x1C,
    0x83,
    0xAA,
    HX8357D_SETSTBA,
    6,
    0x50,
    0x50,
    0x01,
    0x3C,
    0x1E,
    0x08,
    // MEME GAMMA HERE
    HX8357D_SETCYC,
    7,
    0x02,
    0x40,
    0x00,
    0x2A,
    0x2A,
    0x0D,
    0x78,
    HX8357_COLMOD,
    1,
    0x55,
    HX8357_MADCTL,
    1,
    0xC0,
    HX8357_TEON,
    1,
    0x00,
    HX8357_TEARLINE,
    2,
    0x00,
    0x02,
    HX8357_SLPOUT,
    0,
    TFTLCD_DELAY,
    150,
    HX8357_DISPON,
    0,
    TFTLCD_DELAY,
    50,
};




void WR_STROBE()
{
  digitalWriteFast(35, LOW);
  delayNanoseconds(1);
  digitalWriteFast(35, HIGH);
}

void RD_ACTIVE()
{
  digitalWriteFast(36, LOW);
}
void RD_IDLE()
{
  digitalWriteFast(36, HIGH);
}

void CS_IDLE()
{
  digitalWriteFast(33, HIGH);
}

void CS_ACTIVE()
{
  digitalWriteFast(33, LOW);
}

void WR_IDLE()
{
  digitalWriteFast(35, HIGH);
}

void WR_ACTIVE()
{
  digitalWriteFast(35, LOW);
}

void CD_COMMAND()
{
  digitalWriteFast(34, LOW);
}

void CD_DATA()
{
  digitalWriteFast(34, HIGH);
}

void reset()
{
  CS_IDLE();
  WR_IDLE();
  RD_IDLE();

  digitalWriteFast(37, LOW);
  delay(2);
  digitalWriteFast(37, HIGH);

  CS_ACTIVE();
  CD_COMMAND();

  write8inline(0x00);

  for (int i = 0; i < 3; i++)
  {
    WR_STROBE(); // Three extra 0x00s
  }
  CS_IDLE();
}

void begin()
{
  uint8_t i = 0;

  reset();

  delay(200);

  CS_ACTIVE();
  while (i < sizeof(HX8357D_regValues))
  {
    uint8_t r = pgm_read_byte(&HX8357D_regValues[i++]);
    uint8_t len = pgm_read_byte(&HX8357D_regValues[i++]);
    if (r == TFTLCD_DELAY)
    {
      delay(len);
    }
    else
    {
      // Serial.print("Register $"); Serial.print(r, HEX);
      // Serial.print(" datalen "); Serial.println(len);

      CS_ACTIVE();
      CD_COMMAND();
      write8inline(r);
      CD_DATA();
      for (uint8_t d = 0; d < len; d++)
      {
        uint8_t x = pgm_read_byte(&HX8357D_regValues[i++]);
        write8inline(x);
      }
      CS_IDLE();
    }
  }
}

void setWriteDirInline()
{
  for (int i = 13; i <= 20; i++)
  {
    pinMode(i, OUTPUT);
  }
}
void setReadDirInline()
{
  for (int i = 13; i <= 20; i++)
  {
    pinMode(i, INPUT);
  }
}

void write8inline(uint8_t d)
{

  for (int i = 13; i <= 20; i++)
  {
    digitalWriteFast(i, (d & (1 << (i - 13))));
  }
  WR_STROBE();
}

uint8_t read8inline()
{
  uint8_t result = 0;
  RD_ACTIVE();
  // delay(7);
  for (int i = 13; i <= 20; i++)
  {
    uint8_t temp = digitalRead(i);
    result |= (temp << (i - 13));
  }
  RD_IDLE();
  return result;
}

void setup()
{
  Serial.begin(9600);
  CS_IDLE(); // Set all control bits to idle state
  WR_IDLE();
  RD_IDLE();
  CD_DATA();
  digitalWriteFast(37, HIGH); // Reset line

  for (int i = 33; i <= 37; i++)
  {
    pinMode(i, OUTPUT);
  }
  setWriteDirInline(); // set data port for writing
  reset();

  delay(1000);

  uint16_t identifier = readID();
  Serial.print("ID = ");
  Serial.println(identifier, HEX);

  begin();

  Serial.print("Initializing SD card...");
  if (!SD.begin(BUILTIN_SDCARD))
  {
    Serial.println("failed!");
  }
  Serial.println("OK!");
  bmpDraw("jumpers.bmp", 0, 0);
}
void loop()
{
  // delay(100);
  // Serial.println(readID());
  //  bmpDraw("jumpers.bmp", 0, 0);
}

#define BUFFPIXEL 80

void bmpDraw(char *filename, int x, int y)
{

  File bmpFile;
  int bmpWidth, bmpHeight;            // W+H in pixels
  uint8_t bmpDepth;                   // Bit depth (currently must be 24)
  uint32_t bmpImageoffset;            // Start of image data in file
  uint32_t rowSize;                   // Not always = bmpWidth; may have padding
  uint8_t sdbuffer[3 * BUFFPIXEL];    // pixel in buffer (R+G+B per pixel)
  uint16_t lcdbuffer[BUFFPIXEL];      // pixel out buffer (16-bit per pixel)
  uint8_t buffidx = sizeof(sdbuffer); // Current position in sdbuffer
  boolean goodBmp = false;            // Set to true on valid header parse
  boolean flip = true;                // BMP is stored bottom-to-top
  int w, h, row, col;
  uint8_t r, g, b;
  uint32_t pos = 0, startTime = millis();
  uint8_t lcdidx = 0;
  boolean first = true;

  if ((x >= TFTWIDTH) || (y >= TFTHEIGHT))
    return;

  Serial.println();
  Serial.print(F("Loading image '"));
  Serial.print(filename);
  Serial.println('\'');
  // Open requested file on SD card
  if ((bmpFile = SD.open(filename)) == NULL)
  {
    Serial.println(F("File not found"));
    return;
  }

  // Parse BMP header
  if (read16(bmpFile) == 0x4D42)
  { // BMP signature
    Serial.println(F("File size: "));
    Serial.println(read32(bmpFile));
    (void)read32(bmpFile);            // Read & ignore creator bytes
    bmpImageoffset = read32(bmpFile); // Start of image data
    Serial.print(F("Image Offset: "));
    Serial.println(bmpImageoffset, DEC);
    // Read DIB header
    Serial.print(F("Header size: "));
    Serial.println(read32(bmpFile));
    bmpWidth = read32(bmpFile);
    bmpHeight = read32(bmpFile);
    if (read16(bmpFile) == 1)
    {                             // # planes -- must be '1'
      bmpDepth = read16(bmpFile); // bits per pixel
      Serial.print(F("Bit Depth: "));
      Serial.println(bmpDepth);
      if ((bmpDepth == 24) && (read32(bmpFile) == 0))
      { // 0 = uncompressed

        goodBmp = true; // Supported BMP format -- proceed!
        Serial.print(F("Image size: "));
        Serial.print(bmpWidth);
        Serial.print('x');
        Serial.println(bmpHeight);

        // BMP rows are padded (if needed) to 4-byte boundary
        rowSize = (bmpWidth * 3 + 3) & ~3;

        // If bmpHeight is negative, image is in top-down order.
        // This is not canon but has been observed in the wild.
        if (bmpHeight < 0)
        {
          bmpHeight = -bmpHeight;
          flip = false;
        }

        // Crop area to be loaded
        w = bmpWidth;
        h = bmpHeight;
        if ((x + w - 1) >= TFTWIDTH)
          w = TFTWIDTH - x;
        if ((y + h - 1) >= TFTHEIGHT)
          h = TFTHEIGHT - y;

        // Set TFT address window to clipped image bounds
        setAddrWindow(x, y, x + w - 1, y + h - 1);

        for (row = 0; row < h; row++)
        { // For each scanline...
          // Seek to start of scan line.  It might seem labor-
          // intensive to be doing this on every line, but this
          // method covers a lot of gritty details like cropping
          // and scanline padding.  Also, the seek only takes
          // place if the file position actually needs to change
          // (avoids a lot of cluster math in SD library).
          if (flip) // Bitmap is stored bottom-to-top order (normal BMP)
            pos = bmpImageoffset + (bmpHeight - 1 - row) * rowSize;
          else // Bitmap is stored top-to-bottom
            pos = bmpImageoffset + row * rowSize;
          if (bmpFile.position() != pos)
          { // Need seek?
            bmpFile.seek(pos);
            buffidx = sizeof(sdbuffer); // Force buffer reload
          }

          for (col = 0; col < w; col++)
          { // For each column...
            // Time to read more pixel data?
            if (buffidx >= sizeof(sdbuffer))
            { // Indeed
              // Push LCD buffer to the display first
              if (lcdidx > 0)
              {
                pushColors(lcdbuffer, lcdidx, first);
                lcdidx = 0;
                first = false;
              }
              bmpFile.read(sdbuffer, sizeof(sdbuffer));
              buffidx = 0; // Set index to beginning
            }

            // Convert pixel from BMP to TFT format
            b = sdbuffer[buffidx++];
            g = sdbuffer[buffidx++];
            r = sdbuffer[buffidx++];
            lcdbuffer[lcdidx++] = color565(r, g, b);
          } // end pixel
        }   // end scanline
        // Write any remaining data to LCD
        if (lcdidx > 0)
        {
          pushColors(lcdbuffer, lcdidx, first);
        }
        Serial.print(F("Loaded in "));
        Serial.print(millis() - startTime);
        Serial.println(" ms");
      } // end goodBmp
    }
  }

  bmpFile.close();
  if (!goodBmp)
    Serial.println(F("BMP format not recognized."));
}

// These read 16- and 32-bit types from the SD card file.
// BMP data is stored little-endian, Arduino is little-endian too.
// May need to reverse subscript order if porting elsewhere.

uint16_t read16(File f)
{
  uint16_t result;
  ((uint8_t *)&result)[0] = f.read(); // LSB
  ((uint8_t *)&result)[1] = f.read(); // MSB
  return result;
}

uint32_t read32(File f)
{
  uint32_t result;
  ((uint8_t *)&result)[0] = f.read(); // LSB
  ((uint8_t *)&result)[1] = f.read();
  ((uint8_t *)&result)[2] = f.read();
  ((uint8_t *)&result)[3] = f.read(); // MSB
  return result;
}
void writeRegister32(uint8_t r, uint32_t d)
{
  CS_ACTIVE();
  CD_COMMAND();
  write8inline(r);
  CD_DATA();
  delayMicroseconds(10);
  write8inline(d >> 24);
  delayMicroseconds(10);
  write8inline(d >> 16);
  delayMicroseconds(10);
  write8inline(d >> 8);
  delayMicroseconds(10);
  write8inline(d);
  CS_IDLE();
}

void setAddrWindow(int x1, int y1, int x2, int y2)
{
  CS_ACTIVE();

  uint32_t t;

  t = x1;
  t <<= 16;
  t |= x2;
  writeRegister32(0X2A, t); // HX8357D uses same registers!
  t = y1;
  t <<= 16;
  t |= y2;
  writeRegister32(0X2B, t); // HX8357D uses same registers!

  CS_IDLE();
}

void pushColors(uint16_t *data, uint8_t len, boolean first)
{
  uint16_t color;
  uint8_t hi, lo;
  CS_ACTIVE();
  if (first == true)
  { // Issue GRAM write command only on first call
    CD_COMMAND();
    write8inline(0x2C);
  }
  CD_DATA();
  while (len--)
  {
    color = *data++;
    hi = color >> 8;  // Don't simplify or merge these
    lo = color;       // lines, there's macro shenanigans
    write8inline(hi); // going on.
    write8inline(lo);
  }
  CS_IDLE();
}
uint16_t color565(uint8_t r, uint8_t g, uint8_t b)
{
  return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
}

// Ditto with the read/write port directions, as above.
uint16_t readID(void)
{
  uint16_t id;

  // retry a bunch!
  for (int i = 0; i < 5; i++)
  {

    id = (uint16_t)readReg(0xD3);
    delayMicroseconds(50);
    if (id == 0x9341)
    {

      return id;
    }
  }

  uint8_t hi, lo;

  /*
  for (uint8_t i=0; i<128; i++) {
    Serial.print("$"); Serial.print(i, HEX);
    Serial.print(" = 0x"); Serial.println(readReg(i), HEX);
  }
  */

  if (readReg(0x04) == 0x8000)
  { // eh close enough
    // setc!
    /*
        Serial.println("!");
        for (uint8_t i = 0; i < 254; i++)
        {
          Serial.print("$");
          Serial.print(i, HEX);
          Serial.print(" = 0x");
          Serial.println(readReg(i), HEX);
        }
    */
    writeRegister24(HX8357D_SETC, 0xFF8357);
    delay(300);
    // Serial.println(readReg(0xD0), HEX);
    if (readReg(0xD0) == 0x990000)
    {
      return 0x8357;
    }
  }

  CS_ACTIVE();
  CD_COMMAND();
  write8inline(0x00);
  WR_STROBE();        // Repeat prior byte (0x00)
  setReadDirInline(); // Set up LCD data port(s) for READ operations
  CD_DATA();
  hi = read8inline();
  lo = read8inline();

  setWriteDirInline(); // Restore LCD data port(s) to WRITE configuration
  CS_IDLE();

  id = hi;
  id <<= 8;
  id |= lo;
  return id;
}

uint32_t readReg(uint8_t r)
{
  uint32_t id;
  uint8_t x;

  // try reading register #4
  CS_ACTIVE();
  CD_COMMAND();
  write8inline(r);
  setReadDirInline(); // Set up LCD data port(s) for READ operations
  CD_DATA();
  delayMicroseconds(50);
  x = read8inline();
  id = x;   // Do not merge or otherwise simplify
  id <<= 8; // these lines.  It's an unfortunate
  x = read8inline();
  id |= x;  // shenanigans that are going on.
  id <<= 8; // these lines.  It's an unfortunate
  x = read8inline();
  id |= x;  // shenanigans that are going on.
  id <<= 8; // these lines.  It's an unfortunate
  x = read8inline();
  id |= x; // shenanigans that are going on.
  CS_IDLE();
  setWriteDirInline(); // Restore LCD data port(s) to WRITE configuration

  // Serial.print("Read $"); Serial.print(r, HEX);
  // Serial.print(":\t0x"); Serial.println(id, HEX);
  return id;
}

void writeRegister24(uint8_t r, uint32_t d)
{
  CS_ACTIVE();
  CD_COMMAND();
  write8inline(r);
  CD_DATA();
  delayMicroseconds(10);
  write8inline(d >> 16);
  delayMicroseconds(10);
  write8inline(d >> 8);
  delayMicroseconds(10);
  write8inline(d);
  CS_IDLE();
}