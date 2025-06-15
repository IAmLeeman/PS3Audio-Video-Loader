// SUPAHAXOR
// PS3 DOKI DOKI LITERATURE CLUB PORT
// 15/06/2025
// Initialisation Script // V2

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
#include <rsx/commands.h>

#include <io/pad.h>
#include "rsxutil.h"

#include <string.h>
#include <audio/audio.h>




#define IMAGE_WIDTH 1280
#define IMAGE_HEIGHT 720
#define MAX_BUFFERS 2
#define AUDIO_BUFFER_SAMPLES 512
#define NUM_AUDIO_BUFFERS 4

#define AUDIO_NUM_BLOCKS 4
//#define NUM_AUDIO_BUFFERS 8

static int currentBuffer = 0;
static u32 pos = 0;

s16* audio_buf[NUM_AUDIO_BUFFERS];
u8 *raw_audio_data = NULL;
u32 offset = 0;
u32 total_audio_size = 0;

int currentAudioBuffer = 0;
  

uint8_t *rawTexture = NULL;
gcmContextData *context; // RSX global command buffer CONTEXT

gcmSurface surface;

typedef struct {
  u32 *ptr;
  u32 offset;
  u32 height;
  u32 width;
  u32 pitch;
  u32 size;
  } myRsxBuffer;

myRsxBuffer buffers[MAX_BUFFERS];

void SetupBuffers() {
  videoState state;
  videoGetState(0,0, &state);
  videoResolution res;
  videoGetResolution(state.displayMode.resolution, &res);
  
  printf("Setting up video resolution: %d x %d\n", res.width, res.height);
  
  for (int i = 0; i < MAX_BUFFERS; i++) {
    buffers[i].width = res.width;
    buffers[i].height = res.height;
    buffers[i].pitch = res.width * 4;
    buffers[i].size = buffers[i].pitch * res.height;
    buffers[i].ptr = rsxMemalign(64, buffers[i].size);
    rsxAddressToOffset(buffers[i].ptr, &buffers[i].offset);
    gcmSetDisplayBuffer(i, buffers[i].offset, buffers[i].pitch, res.width, res.height);
    printf("Buffer %d: Width = %d, height = %d, pitch = %d, size = %d\n", i, buffers[i].width, buffers[i].height,         buffers[i].pitch, buffers[i].size);
  }
  
}
void INITRSX(){
  void *host_addr = memalign(1024 * 1024, 1024 * 1024);
  rsxInit(&context, 0x10000, 1024 * 1024, host_addr); // 64kb command buffer. // 1MB IO buffer.
  if (!rsxInit){
    printf("rsxInit failed!\n");
    return;
  }
  else {
    printf("rsxInit completed!\n");
  }
}

int AUDIOINIT(const char *filepath) {
  
  audioPortParam params;
  audioPortConfig config;
  u32 portNum;
  u32 i;
  
  s32 ret = audioInit();
  
  if (ret != 0) {
    printf("AUDIOINIT failed %d\n", ret);
    return 0;
  }
  
  params.numChannels = AUDIO_PORT_2CH;
  params.numBlocks = AUDIO_BLOCK_8;
  params.attrib = AUDIO_PORT_INITLEVEL;
  params.level = 1.0f;
  ret = audioPortOpen(&params,&portNum);
  
  if (ret < 0) {
    printf("audioPortOpen failed %d\n", ret);
    audioQuit();
    return 0;
  }
  
  FILE *file = fopen(filepath, "rb");
  if (!file) {
    printf("Failed to open raw_audio\n");
    return 0;
  }
  return 1; 
}
/*void PushAudioBuffer(){
  for (int i = 0; i < AUDIO_BLOCK_SAMPLES; i++){
      audio_buf[i*2 + 0] = (f32)*((s16*)&tada48_16_2_raw[pos])/32768.0f;
      audio_buf[i*2 + 1] = (f32)*((s16*)&tada48_16_2_raw[pos + 2])/32768.0f;
      pos += 4;
      
    if (audio_buf[i] == NULL){
      printf("Failed to allocate audio buffer %d\n");
      return;
    }
  }
}*/

int LoadRawImage(const char *filepath) {
    FILE *file = fopen(filepath, "rb");
    
    if (!file) {
      printf("Failed to open image : %s\n");
      return 0;
    }
    size_t imageSize = IMAGE_WIDTH * IMAGE_HEIGHT * 4;
    rawTexture = (uint8_t*)rsxMemalign(128, imageSize); // Allocate memory in RSX with 128 byte adjustment.
    if (!rawTexture) {
      printf("RSX memory allocation failed - pushing to system memory\n");
      rawTexture = (uint8_t*)malloc(imageSize); // Allocate memory in general RAM.
      if (!rawTexture) {
        fclose(file);
        printf("System Memory allocation failed\n");
        return 0;
        }
    }
    size_t bytesRead = fread(rawTexture, 1, imageSize, file);
    if (bytesRead != imageSize) {
      fclose(file);
      printf("Failed to read full image. Read %zu bytes instead of %zu\n", bytesRead, imageSize);
      return 0;
    }
    
    fclose(file);
    return 1;
}
void TextureSetup() {
  gcmTexture texture;
  texture.width       = IMAGE_WIDTH;
  texture.height      = IMAGE_HEIGHT;
  texture.format      = GCM_TEXTURE_FORMAT_A8R8G8B8;
  texture.mipmap      = 0;
  texture.depth       = 1;
  texture.dimension   = GCM_TEXTURE_DIMS_2D;
  texture.cubemap     = GCM_FALSE;
  texture.pitch       = IMAGE_WIDTH * 4;
  
  texture.remap =
    GCM_TEXTURE_REMAP_TYPE_REMAP << 14 |
    GCM_TEXTURE_REMAP_COLOR_B << 6 |
    GCM_TEXTURE_REMAP_COLOR_G << 4 |
    GCM_TEXTURE_REMAP_COLOR_R << 2 |
    GCM_TEXTURE_REMAP_COLOR_A;

  rsxAddressToOffset(rawTexture, &texture.offset);
  rsxLoadTexture(context, 0, &texture);   // <------------------ BANE OF MY FUCKING EXISTENCE... OH MY!!!
}

void DrawQuad(){
  typedef struct {
    float x, y, z;
    float r, g, b, a;   // Single colours rather than image coordinates, debug only.
} Vertex;

Vertex *vertices = (Vertex *)rsxMemalign(128, sizeof(Vertex) * 6);
if(vertices == NULL) {
  printf("Memory allocation failed for vertices\n");
  return;
  }
else {
  printf("Memory allocation for vertices succeeded\n");
}



vertices[0] = (Vertex){ 0.0f, 0.0f, 0.0f,          1.0f, 0.0f, 0.0f, 1.0f };
vertices[1] = (Vertex){ 1280.0f, 0.0f, 0.0f,       1.0f, 0.0f, 0.0f, 1.0f };
vertices[2] = (Vertex){ 0.0f, 720.0f, 0.0f,        0.0f, 0.0f, 1.0f, 1.0f };
    
vertices[3] = (Vertex){ 1280.0f, 0.0f, 0.0f,       0.0f, 1.0f, 0.0f, 1.0f };
vertices[4] = (Vertex){ 1280.0f, 720.0f, 0.0f,     1.0f, 0.0f, 0.0f, 1.0f };
vertices[5] = (Vertex){ 0.0f, 720.0f, 0.0f,        0.0f, 0.0f, 1.0f, 1.0f };

u32 offset;
rsxAddressToOffset(vertices, &offset);

//rsxTextureControl(context, 0, GCM_TRUE, 0<<8, 12<<8, GCM_TEXTURE_MAX_ANISO_1);
//rsxTextureAddress(context, 0, GCM_TEXTURE_WRAP, GCM_TEXTURE_WRAP, GCM_TEXTURE_CLAMP_TO_EDGE);
//rsxTextureFilter(context, 0, GCM_TEXTURE_LINEAR, GCM_TEXTURE_LINEAR, GCM_TEXTURE_CONVOLUTION_QUINCUNX);

rsxBindVertexArrayAttrib(context, 0, 0, sizeof(Vertex), 3, GCM_VERTEX_DATA_TYPE_F32, GCM_LOCATION_RSX, offset);
rsxBindVertexArrayAttrib(context, 1, 0, sizeof(Vertex), 2, GCM_VERTEX_DATA_TYPE_F32, GCM_LOCATION_RSX, offset + 12);

rsxDrawVertexArray(context, 0, GCM_TYPE_TRIANGLES, 6);

}

void BeginFrame() {
  surface.colorFormat = GCM_SURFACE_A8R8G8B8;
  surface.colorTarget = GCM_SURFACE_TARGET_0;
  surface.colorLocation[0] = GCM_LOCATION_RSX;
  surface.colorOffset[0] = buffers[currentBuffer].offset;
  surface.colorPitch[0] = buffers[currentBuffer].pitch;
  surface.width = buffers[currentBuffer].width;
  surface.height = buffers[currentBuffer].height;
  surface.x = 0;
  surface.y = 0;
  //surface.zcull = 0;
  surface.type = GCM_SURFACE_TYPE_LINEAR;
  //surface.antialias = GCM_SURFACE_CENTER_1;

  rsxSetSurface(context, &surface);

  //rsxSetSurface(context, buffers[currentBuffer].offset); /// <---------- Apparently you also need to define the surface which sort of makes sense.
}
void EndFrame() {
  rsxFlushBuffer(context);
  flip(currentBuffer, 0);
  currentBuffer = !currentBuffer;
}

int main(int argc,char *argv[]){

  //PushAudioBuffer();
  //AUDIOINIT("/dev_hdd0/COMEUPOUTDAWAHTA.raw");
  
  INITRSX();  // Does not crash the PS3.
  LoadRawImage("/dev_hdd0/video/raw_video/m_argb.raw"); // Does not crash the PS3.
  
  TextureSetup(); // Does not crash the PS3.
  SetupBuffers(); // Does not crash the PS3.
  
  //<----------------------------------> Everything above this line works, as in it does not crash the PS3
  
  
  
  while(1){   // Nothing renders but it doesn't crash either.

    waitFlip();
    BeginFrame();
    DrawQuad();   // DEBUG ON
    //usleep(16000);
    EndFrame();
    
    usleep(16000);
    
  }
  return 0;
    
}