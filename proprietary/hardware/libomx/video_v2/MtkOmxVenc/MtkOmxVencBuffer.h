
OMX_ERRORTYPE CheckInputBufferPortAvailbility();
OMX_ERRORTYPE CheckOutputBufferPortAvailbility();

void HandleInputPortPopulated();
void HandleOutputPortPopulated();

OMX_ERRORTYPE InputBufferHeaderAllocate(OMX_BUFFERHEADERTYPE**, OMX_U32, OMX_PTR, OMX_U32);
OMX_ERRORTYPE EpilogueInputBufferHeaderAllocate();

OMX_ERRORTYPE OutputBufferHeaderAllocate(OMX_BUFFERHEADERTYPE**, OMX_U32, OMX_PTR, OMX_U32);
OMX_ERRORTYPE EpilogueOutputBufferHeaderAllocate();

OMX_ERRORTYPE InputBufferHeaderUse(OMX_BUFFERHEADERTYPE**, OMX_U32, OMX_PTR, OMX_U32, OMX_U8*);
OMX_ERRORTYPE EpilogueInputBufferHeaderUse();

OMX_ERRORTYPE OutputBufferHeaderUse(OMX_BUFFERHEADERTYPE**, OMX_U32, OMX_PTR, OMX_U32, OMX_U8*);
OMX_ERRORTYPE EpilogueOutputBufferHeaderUse();

OMX_BOOL AllowToFreeBuffer(OMX_U32, OMX_STATETYPE);
bool bufferReadyState(OMX_STATETYPE);

OMX_ERRORTYPE UnmapInputMemory(OMX_BUFFERHEADERTYPE*);
OMX_ERRORTYPE UnmapOutputMemory(OMX_BUFFERHEADERTYPE*);
OMX_ERRORTYPE releaseOutputNativeHandle(OMX_BUFFERHEADERTYPE*);

OMX_ERRORTYPE FreeInputBuffers(OMX_BUFFERHEADERTYPE*);
OMX_ERRORTYPE FreeOutputBuffers(OMX_BUFFERHEADERTYPE*);
