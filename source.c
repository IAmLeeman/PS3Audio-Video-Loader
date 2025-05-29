// SUPAHAXOR
// PS3 DOKI DOKI LITERATURE CLUB PORT
// 20/04/2025
// Initialisation Script

#include <ppu-lv2.h>

#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <malloc.h>
#include <string.h>
#include <assert.h>
#include <unistd.h>

#include <sysutil/video.h>
#include <rsx/gcm_sys.h>
#include <rsx/rsx.h>
//#include <rsx_program.h>  <----- Likely is integrated in the rsx/rsx.h file now.
#include <rsx/commands.h>

#include <io/pad.h>
#include "rsxutil.h"

#include <string.h>
#include <audio/audio.h>

#define COMMAND_BUFFER_SIZE 0x10000  // 1 * 16^4 in hexadecimal = 65536, roughly 64kb.
//#define HOST_SIZE (1024 * 1024)
#define MAX_BUFFERS 2
#define IMAGE_WIDTH 1024         // Adjust for actual image width before converted to RAW.
#define IMAGE_HEIGHT 768          // Adjust for actual image height before converted to RAW.


uint8_t *raw_texture = NULL;
void* rsx_texture_mem = NULL;
gcmContextData *context = NULL;

#define RAW_IMAGE_PATH "/dev_usb001/video/raw_video/hardbass_raw.raw" // Hard coded path to test codec.
#define RAW_AUDIO_PATH "/dev_usb001/audio/raw_audio/COMEUPOUTDAWAHTA.raw" // Hard coded path to test codec.

//void drawText(const char* text, float y, float x)
//{
    // Load a texture bitmap -- It might work.
    
//}

int loadRawImage(const char *filepath){                     //// DO NOT DELETE THIS - IT ACTUALLY WORKS.
  FILE *file = fopen(filepath, "rb");
  if (!file) {
    printf("Failed to open image: %s\n", filepath);
    return 0;
  }
  
  
  size_t imageSize = IMAGE_WIDTH * IMAGE_HEIGHT * 4;
  raw_texture = (uint8_t*)memalign(128, imageSize);      /// This is the space dedicated in memory for the empty texture.
  if (!raw_texture) {
    fclose(file);
    printf("Memory allocation failed\n");
    return 0;
  }
  
  fread(raw_texture, 1, imageSize, file);
  fclose(file);
  
  printf("Loaded raw image: %s (%u bytes)\n", filepath, imageSize);
  return 1;

}

int printImage(){

  rsx_texture_mem = rsxMemalign(128, IMAGE_WIDTH * IMAGE_HEIGHT * 4);
  //rsx_texture_mem = rsxMap(rsx_texture_mem, IMAGE_WIDTH * IMAGE_HEIGHT * 4);
  
  if (!rsx_texture_mem){
    printf("RSX memory allocation failed\n");
    return 0;
  }
    
  if (!raw_texture) {
    printf("Raw texture is NULL\n");
    return 0;
  }
    
  memcpy(rsx_texture_mem, raw_texture, IMAGE_WIDTH * IMAGE_HEIGHT * 4);
  printf("Image loaded into RSX memory\n");
  
  return 1;
  
}

void drawFrame(rsxBuffer *buffer, long frame) {
  s32 i, j;
  for(i = 0; i < buffer->height; i++) {
    s32 color = (i / (buffer->height * 1.0) * 256);
    // This should make a nice black to green gradient
    color = (color << 8) | ((frame % 255) << 16);
    for(j = 0; j < buffer->width; j++)
      buffer->ptr[i* buffer->width + j] = color;
  }
}

void myLoadTexture(gcmContextData *context, gcmTexture *tex, const void *src){
  
  u32 offset;
  //u32 location = gcmGetMemoryLocation((const void*)src);
  //printf("[DEBUG] Memory location: %s\n", location == GCM_LOCATION_RSX ? "RSX" : "CPU");
  
  rsxAddressToOffset((void*)src, &offset);
  tex->offset = offset;
  
}
void drawTexture(){
  
  //rsxTexture texture;           <------- Remnant of the official SDK.
  
  gcmTexture texture;
  
  texture.width = IMAGE_WIDTH;
  texture.mipmap = 1;
  texture.depth = 1;
  texture.height = IMAGE_HEIGHT;
  texture.format = GCM_TEXTURE_FORMAT_A8R8G8B8;        // RGBA Format
  texture.dimension = GCM_TEXTURE_DIMS_2D;
  texture.cubemap = GCM_FALSE;
  //texture.location = GCM_LOCATION_RSX;              // Unneeded.
  texture.pitch = IMAGE_WIDTH * 4;
  texture.remap =
      GCM_TEXTURE_REMAP_TYPE_REMAP << 14 |
      GCM_TEXTURE_REMAP_COLOR_B << 6 |
      GCM_TEXTURE_REMAP_COLOR_G << 4 |
      GCM_TEXTURE_REMAP_COLOR_R << 2 |
      GCM_TEXTURE_REMAP_COLOR_A;
  
  printf("[DEBUG] Texture struct initialised\n");
  
  myLoadTexture(context, &texture, rsx_texture_mem);
  printf("[DEBUG] Offset applied with myLoadTexture\n");
  
  rsxLoadTexture(context, 0, &texture);  // <-------- This line is crashing the system.
  printf("[DEBUG] rsxLoadTexture completed successfully\n");
  
  //rsxSetTextureWrapMode(context, 0, GCM_TEXTURE_REPEAT, GCM_TEXTURE_REPEAT, GCM_TEXTURE_REPEAT);
  //rsxSetTextureFilter(context, 0, GCM_TEXTURE_LINEAR, GCM_TEXTURE_LINEAR, GCM_TEXTURE_CONVOLUTION_QUINCUNX);
  
  //rsxSetRenderTarget(context, NULL); // NULL might represent the main screen or default framebuffer
  //rsxSetTextureAddress(0, GCM_TEXTURE_REPEAT, GCM_TEXTURE_REPEAT, 0,0,0); <------ Probably a remnant.
  
  typedef struct {
    float x, y, z;
    float u, v;
  } Vertex;
  
  // Draws a 1024 x 768 texture in top left corner
  
  Vertex quad[6] = {
    {0.0f, 0.0f, 0.0f,    0.0f, 0.0f},    // You need to stitch two triangles together as we just fuckin' love triangles.
    {768.0f, 0.0f, 0.0f,  1.0f, 0.0f},      // no really, it's far more resource effective.
    {0.0f, 1024.0f, 0.0f, 0.0f, 1.0f},
    
    {0.0f, 768.0f, 0.0f,  0.0f, 0.0f},
    {768.0f, 1024.0f, 0.0f, 1.0f, 1.0f},
    {0.0f, 1024.0f, 0.0f, 0.0f, 1.0f},
    };
  
  
  
  
  // rsxSetTexture(context, 0, &texture, GCM_TEXTURE_REPEAT, GCM_TEXTURE_REPEAT, 0); // <------- Maybe a remnant, or maybe ChatGPT is hallucinating.
  
  
  
  //texture.remap =                                     <----------- REMNANTS, ALWAYS HAS BEEN.
    //        GCM_TEXTURE_REMAP_ORDER_XYXY |
      //      GCM_TEXTURE_REMAP_FROM_AAAA |
        //    GCM_TEXTURE_REMAP_TO_8888;
  
  
  
  //texture.data = rsx_texture_mem; // Texture data currently stored in RSX memory.
  
  //rsxBindTexture(0, &texture);      <----------- Remnant, don't worry fam, I'll build it as gcmBindTexture
  
  //rsxDrawQuad(0,0, IMAGE_WIDTH, IMAGE_HEIGHT); <----- Another remnant, unlike Raytheon I'll actually fix this shit.
}



int main(s32 argc, const char* argv[])
{
  gcmContextData *context;
  void *host_addr = memalign(1024*1024, HOST_SIZE);
  rsxInit(&context, COMMAND_BUFFER_SIZE, HOST_SIZE, host_addr);
  rsxBuffer buffers[MAX_BUFFERS];
  
  int currentBuffer = 0;
  
  //videoGetState(0, 0, &state);
  //videoGetResolution(state.displayMode.resolution, &res);   <--------------- Won't work whatsoever
  
  u32 width = 1280;
  u32 height = 720;
  
  //printf("Hardcoded resolution: %u x %u\n", width, height); <--------------- Optimisations deem this line unnecessary.
  
  loadRawImage(RAW_IMAGE_PATH);         // Loads image to memory
  printImage();                         // Prints image to screen
  
  drawTexture();
  
  /*padInfo padinfo;
  padData paddata;
  u16 width;
  u16 height;
  int i;
	
  /* Allocate a 1Mb buffer, alligned to a 1Mb boundary                          
   * to be our shared IO memory with the RSX.
  host_addr = memalign (1024*1024, HOST_SIZE);
  context = initScreen (host_addr, HOST_SIZE);
  ioPadInit(7);

  getResolution(&width, &height);
  for (i = 0; i < MAX_BUFFERS; i++)
    makeBuffer( &buffers[i], width, height, i);

  flip(context, MAX_BUFFERS - 1);

  long frame = 0; // To keep track of how many frames we have rendered.
	
  // Ok, everything is setup. Now for the main loop.
  while(1){
    // Check the pads.
    ioPadGetInfo(&padinfo);
    for(i=0; i<MAX_PADS; i++){
      if(padinfo.status[i]){
	ioPadGetData(i, &paddata);
				
	if(paddata.BTN_START){
	  goto end;
	}
      }
			
    }

    waitFlip(); // Wait for the last flip to finish, so we can draw to the old buffer
    drawFrame(&buffers[currentBuffer], frame++); // Draw into the unused buffer
    flip(context, buffers[currentBuffer].id); // Flip buffer onto screen

    currentBuffer++;
    if (currentBuffer >= MAX_BUFFERS)
      currentBuffer = 0;
  }
  
 end:

  gcmSetWaitFlip(context);
  for (i = 0; i < MAX_BUFFERS; i++)
    rsxFree(buffers[i].ptr);

  rsxFinish(context, 1);
  free(host_addr);
  ioPadEnd();
	
  */return 0;
}