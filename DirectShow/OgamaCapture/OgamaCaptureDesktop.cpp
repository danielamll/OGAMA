#include <streams.h>
#include "OgamaCapture.h"
#include "OgamaCaptureGuids.h"
#include "DibHelper.h"
#include <wmsdkidl.h>

/**********************************************
 *
 *  COgamaCapturePinDesktop Class
 *
 *
 **********************************************/
#define MIN(a,b)  ((a) < (b) ? (a) : (b))  // danger! can evaluate "a" twice.

DWORD globalStart; // for some debug performance benchmarking
int countMissed = 0;
long fastestRoundMillis = 1000000;
long sumMillisTook = 0;

#ifdef _DEBUG 
int show_performance = 1;
#else
int show_performance = 0;
#endif

// the default child constructor...
COgamaCapturePinDesktop::COgamaCapturePinDesktop(HRESULT *phr, COgamaCaptureDesktop *pFilter)
  : CSourceStream(NAME("Ogama Capture child/pin"), phr, pFilter, L"Capture"),
  m_bReReadRegistry(0),
  m_bDeDupe(0),
  m_iFrameNumber(0),
  pOldData(NULL),
  m_bConvertToI420(false),
  m_pParent(pFilter),
  m_bFormatAlreadySet(false),
  hRawBitmap(NULL),
  m_bUseCaptureBlt(false),
  previousFrameEndTime(0),
  m_MonitorIndex(0)
{
  // Get the device context of the main display, just to get some metrics for it...
  globalStart = GetTickCount();

  // create a DC for the screen 
  if (m_MonitorIndex == 0)
  {
    hScrDc = CreateDC(TEXT("DISPLAY"), TEXT("DISPLAY"), NULL, NULL);
  }
  else
  {
    hScrDc = CreateDC(TEXT("\\\\.\\DISPLAY2"), TEXT("\\\\.\\DISPLAY2"), NULL, NULL);
  }

  //m_iScreenBitDepth = GetTrueScreenDepth(hScrDc);
  ASSERT(hScrDc != 0); // failure...

  // Get the dimensions of the main desktop window as the default
  m_rScreen.left = m_rScreen.top = 0;
  m_rScreen.right = GetDeviceCaps(hScrDc, HORZRES); // NB this *fails* for dual monitor support currently... but we just get the wrong width by default, at least with aero windows 7 both can capture both monitors
  m_rScreen.bottom = GetDeviceCaps(hScrDc, VERTRES);

  // now read some custom settings...
  WarmupCounter();
  reReadCurrentPosition(0);

  int config_width = 0; //read_config_setting(TEXT("capture_width"), 0);
  ASSERT(config_width >= 0); // negatives not allowed...
  int config_height = 0; //read_config_setting(TEXT("capture_height"), 0);
  ASSERT(config_height >= 0); // negatives not allowed, if it's set :)

  if (config_width > 0) {
    int desired = m_rScreen.left + config_width;
    //int max_possible = m_rScreen.right; // disabled check until I get dual monitor working. or should I allow off screen captures anyway?
    //if(desired < max_possible)
    m_rScreen.right = desired;
    //else
    //	m_rScreen.right = max_possible;
  }
  else {
    // leave full screen
  }

  m_iCaptureConfigWidth = m_rScreen.right - m_rScreen.left;
  ASSERT(m_iCaptureConfigWidth > 0);

  if (config_height > 0) {
    int desired = m_rScreen.top + config_height;
    //int max_possible = m_rScreen.bottom; // disabled, see above.
    //if(desired < max_possible)
    m_rScreen.bottom = desired;
    //else
    //	m_rScreen.bottom = max_possible;
  }
  else {
    // leave full screen
  }
  m_iCaptureConfigHeight = m_rScreen.bottom - m_rScreen.top;
  ASSERT(m_iCaptureConfigHeight > 0);

  m_iStretchToThisConfigWidth = 0; //read_config_setting(TEXT("stretch_to_width"), 0);
  m_iStretchToThisConfigHeight = 0; //read_config_setting(TEXT("stretch_to_height"), 0);
  m_iStretchMode = 0; //read_config_setting(TEXT("stretch_mode_high_quality_if_1"), 0);
  ASSERT(m_iStretchToThisConfigWidth >= 0 && m_iStretchToThisConfigHeight >= 0 && m_iStretchMode >= 0); // sanity checks

  m_bUseCaptureBlt = 0; //read_config_setting(TEXT("capture_transparent_windows_with_mouse_blink_only_non_aero_if_1"), 0) == 1;

  // default 30 fps...hmm...
  int config_max_fps = 30; //read_config_setting(TEXT("default_max_fps"), 30); // TODO allow floats [?] when ever requested
  ASSERT(config_max_fps >= 0);

  // m_rtFrameLength is also re-negotiated later...
  m_rtFrameLength = UNITS / config_max_fps;

  if (is_config_set_to_1(TEXT("track_new_x_y_coords_each_frame_if_1"))) {
    m_bReReadRegistry = 1; // takes 0.416880ms, but I thought it took more when I made it off by default :P
  }
  if (is_config_set_to_1(TEXT("dedup_if_1"))) {
    m_bDeDupe = 1; // takes 10 or 20ms...but useful to me! :)
  }
  m_millisToSleepBeforePollForChanges = read_config_setting(TEXT("millis_to_sleep_between_poll_for_dedupe_changes"), 10);

  wchar_t out[1000];
  swprintf(out, 1000, L"default/from reg read config as: %dx%d -> %dx%d (%dtop %db %dl %dr) %dfps, dedupe? %d, millis between dedupe polling %d, m_bReReadRegistry? %d \n",
    m_iCaptureConfigHeight, m_iCaptureConfigWidth, getCaptureDesiredFinalHeight(), getCaptureDesiredFinalWidth(), m_rScreen.top, m_rScreen.bottom, m_rScreen.left, m_rScreen.right, config_max_fps, m_bDeDupe, m_millisToSleepBeforePollForChanges, m_bReReadRegistry);

  LocalOutput(L"warmup the debugging message system");
  __int64 measureDebugOutputSpeed = StartCounter();
  LocalOutput(out);
  LocalOutput("writing a large-ish debug itself took: %.02Lf ms", GetCounterSinceStartMillis(measureDebugOutputSpeed));
  set_config_string_setting(L"last_init_config_was", out);
}

STDMETHODIMP COgamaCapturePinDesktop::get_Monitor(int* index)
{
  CAutoLock foo(&m_cSharedState);
  *index = m_MonitorIndex;
  return NOERROR;
}

STDMETHODIMP COgamaCapturePinDesktop::set_Monitor(int index)
{
  CAutoLock foo(&m_cSharedState);
  m_MonitorIndex = index;
  UpdateTargetScreen();
  return NOERROR;
}

STDMETHODIMP COgamaCapturePinDesktop::get_Framerate(int* framerate)
{
  CAutoLock foo(&m_cSharedState);
  *framerate = (int)(UNITS / m_rtFrameLength);
  return NOERROR;
}

STDMETHODIMP COgamaCapturePinDesktop::set_Framerate(int framerate)
{
  CAutoLock foo(&m_cSharedState);
  REFERENCE_TIME newFPS = UNITS / framerate;
  m_rtFrameLength = newFPS;
  return NOERROR;
}

STDMETHODIMP COgamaCapturePinDesktop::UpdateTargetScreen(void)
{
  // Free resources if applicable
  ReleaseScreen();

  // create a DC for the screen 
  if (m_MonitorIndex == 0)
  {
    hScrDc = CreateDC(TEXT("DISPLAY"), TEXT("DISPLAY"), NULL, NULL);
  }
  else
  {
    hScrDc = CreateDC(TEXT("\\\\.\\DISPLAY2"), TEXT("\\\\.\\DISPLAY2"), NULL, NULL);
  }

  //m_MemoryDC = CreateCompatibleDC(m_ScreenDC);

  // Get the dimensions of the main desktop window as the default
  m_rScreen.left = m_rScreen.top = 0;
  m_rScreen.right = GetDeviceCaps(hScrDc, HORZRES); // NB this *fails* for dual monitor support currently... but we just get the wrong width by default, at least with aero windows 7 both can capture both monitors
  m_rScreen.bottom = GetDeviceCaps(hScrDc, VERTRES);

  m_iCaptureConfigWidth = m_rScreen.right - m_rScreen.left;
  ASSERT(m_iCaptureConfigWidth > 0);
  m_iCaptureConfigHeight = m_rScreen.bottom - m_rScreen.top;
  ASSERT(m_iCaptureConfigHeight > 0);

  //// create a bitmap compatible with the screen DC
  //HBITMAP bm = CreateCompatibleBitmap(hScrDc, m_rScreen.right, m_rScreen.bottom);
  AM_MEDIA_TYPE *pmt = NULL;
  int  hr = GetFormat(&pmt);

  int iCount, iSize;
  BYTE *pSCC = NULL;
  hr = GetNumberOfCapabilities(&iCount, &iSize);
  pSCC = new BYTE[iSize];
  hr = GetStreamCaps(0, &pmt, pSCC);
  if (hr == S_OK)
  {
    //VIDEOINFOHEADER *pvi = (VIDEOINFOHEADER *)pmt->pbFormat;
    //pvi->bmiHeader.biWidth = m_iCaptureConfigWidth;
    //pvi->bmiHeader.biHeight = m_iCaptureConfigHeight;
    hr = SetFormat(pmt);
    if (FAILED(hr))
    {
      // TODO: Error handling.
    }

    DeleteMediaType(pmt);
  }
  delete[] pSCC;


  //AM_MEDIA_TYPE pSampleGrabber_pmt;
  //ZeroMemory(&pSampleGrabber_pmt, sizeof(AM_MEDIA_TYPE));
  //pSampleGrabber_pmt.majortype = MEDIATYPE_Video;
  //pSampleGrabber_pmt.subtype = MEDIASUBTYPE_RGB24;
  //pSampleGrabber_pmt.formattype = FORMAT_VideoInfo;
  //pSampleGrabber_pmt.bFixedSizeSamples = TRUE;
  //long sampleSize = m_rScreen.right * m_rScreen.bottom * (24 / 8);
  //pSampleGrabber_pmt.cbFormat = 88;
  //pSampleGrabber_pmt.lSampleSize = sampleSize;
  //pSampleGrabber_pmt.bTemporalCompression = TRUE;
  //VIDEOINFOHEADER pSampleGrabber_format;
  //ZeroMemory(&pSampleGrabber_format, sizeof(VIDEOINFOHEADER));
  //pSampleGrabber_format.AvgTimePerFrame = m_rtFrameLength;
  //pSampleGrabber_format.bmiHeader.biSize = sampleSize;
  //pSampleGrabber_format.bmiHeader.biWidth = m_rScreen.right;
  //pSampleGrabber_format.bmiHeader.biHeight = m_rScreen.bottom;
  //pSampleGrabber_format.bmiHeader.biPlanes = 1;
  //pSampleGrabber_format.bmiHeader.biBitCount = 24;
  //pSampleGrabber_format.bmiHeader.biCompression = BI_RGB;
  //pSampleGrabber_format.bmiHeader.biSizeImage = 0;
  //pSampleGrabber_pmt.pbFormat = (BYTE*)&pSampleGrabber_format;
  //SetFormat(pmt);

  return S_OK;
}

STDMETHODIMP COgamaCapturePinDesktop::ReleaseScreen(void)
{
  if (hScrDc != NULL)
  {
    DeleteDC(hScrDc);
  }

  if (hScrDc != NULL)
  {
    DeleteDC(hScrDc);
  }

  //if (m_ScreenBitmap != NULL)
  //{
  //	DeleteObject(m_ScreenBitmap);
  //}

  return S_OK;
}

wchar_t out[1000];

HRESULT COgamaCapturePinDesktop::FillBuffer(IMediaSample *pSample)
{
  LocalOutput("video frame requested");

  __int64 startThisRound = StartCounter();
  BYTE *pData;

  CheckPointer(pSample, E_POINTER);
  if (m_bReReadRegistry) {
    reReadCurrentPosition(1);
  }

  // Access the sample's data buffer
  pSample->GetPointer(&pData);

  // Make sure that we're still using video format
  ASSERT(m_mt.formattype == FORMAT_VideoInfo);

  VIDEOINFOHEADER *pVih = (VIDEOINFOHEADER*)m_mt.pbFormat;

  // for some reason the timings are messed up initially, as there's no start time at all for the first frame (?) we don't start in State_Running ?
  // race condition?
  // so don't do some calculations unless we're in State_Running
  FILTER_STATE myState;
  CSourceStream::m_pFilter->GetState(INFINITE, &myState);
  bool fullyStarted = myState == State_Running;

  boolean gotNew = false;
  while (!gotNew) {

    CopyScreenToDataBlock(hScrDc, pData, (BITMAPINFO *)&(pVih->bmiHeader), pSample);

    if (m_bDeDupe) {
      if (memcmp(pData, pOldData, pSample->GetSize()) == 0) { // took desktop:  10ms for 640x1152, still 100 fps uh guess...
        Sleep(m_millisToSleepBeforePollForChanges);
      }
      else {
        gotNew = true;
        memcpy( /* dest */ pOldData, pData, pSample->GetSize()); // took 4ms for 640x1152, but it's worth it LOL.
        // LODO memcmp and memcpy in the same loop LOL.
      }
    }
    else {
      // it's always new for everyone else!
      gotNew = true;
    }
  }
  // capture how long it took before we add in our own arbitrary delay to enforce fps...
  long double millisThisRoundTook = GetCounterSinceStartMillis(startThisRound);
  fastestRoundMillis = min(millisThisRoundTook, fastestRoundMillis); // keep stats :)
  sumMillisTook += millisThisRoundTook;

  CRefTime now;
  CRefTime endFrame;
  CSourceStream::m_pFilter->StreamTime(now);
  LocalOutput("now is %llu , previousframeend %llu", now, previousFrameEndTime);
  // wait until we "should" send this frame out...
  if ((now > 0) && (now < previousFrameEndTime)) { // now > 0 to accomodate for if there is no reference graph clock at all...also boot strap time ignore it :P
    while (now < previousFrameEndTime) { // guarantees monotonicity too :P
      LocalOutput("sleeping because %llu < %llu", now, previousFrameEndTime);
      Sleep(1);
      CSourceStream::m_pFilter->StreamTime(now);
    }
    // avoid a tidge of creep since we sleep until [typically] just past the previous end.
    endFrame = previousFrameEndTime + m_rtFrameLength;
    previousFrameEndTime = endFrame;

  }
  else {
    // if there's no reference clock, it will "always" miss a frame
    if (show_performance) {
      if (now == 0)
        LocalOutput("probable none reference clock, streaming fastly");
      else
        LocalOutput("it missed a frame--can't keep up %d %llu %llu", countMissed++, now, previousFrameEndTime); // we don't miss time typically I don't think, unless de-dupe is turned on, or aero, or slow computer, buffering problems downstream, etc.
    }
    // have to add a bit here, or it will always be "it missed some time" for the next round...forever!
    endFrame = now + m_rtFrameLength;
    // most of this stuff I just made up because it "sounded right"
    //LocalOutput("checking to see if I can catch up again now: %llu previous end: %llu subtr: %llu %i", now, previousFrameEndTime, previousFrameEndTime - m_rtFrameLength, previousFrameEndTime - m_rtFrameLength);
    if (now > (previousFrameEndTime - (long long)m_rtFrameLength)) { // do I even need a long long cast?
      // let it pretend and try to catch up, it's not quite a frame behind
      previousFrameEndTime = previousFrameEndTime + m_rtFrameLength;
    }
    else {
      endFrame = now + m_rtFrameLength / 2; // ?? seems to not hurt, at least...I guess
      previousFrameEndTime = endFrame;
    }

  }

  previousFrameEndTime = max(0, previousFrameEndTime);// avoid startup negatives, which would kill our math on the next loop...

  //LocalOutput("marking frame with timestamps: %llu %llu", now, endFrame);

  pSample->SetTime((REFERENCE_TIME *)&now, (REFERENCE_TIME *)&endFrame);
  //pSample->SetMediaTime((REFERENCE_TIME *)&now, (REFERENCE_TIME *) &endFrame); 

  if (fullyStarted) {
    m_iFrameNumber++;
  }

  // Set TRUE on every sample for uncompressed frames http://msdn.microsoft.com/en-us/library/windows/desktop/dd407021%28v=vs.85%29.aspx
  pSample->SetSyncPoint(TRUE);

  // only set discontinuous for the first...I think...
  pSample->SetDiscontinuity(m_iFrameNumber <= 1);

  // the swprintf costs like 0.04ms (25000 fps LOL)
  double m_fFpsSinceBeginningOfTime = ((double)m_iFrameNumber) / (GetTickCount() - globalStart) * 1000;
  swprintf(out, L"done video frame! total frames: %d this one %dx%d -> (%dx%d) took: %.02Lfms, %.02f ave fps (%.02f is the theoretical max fps based on this round, ave. possible fps %.02f, fastest round fps %.02f, negotiated fps %.06f), frame missed %d",
    m_iFrameNumber, m_iCaptureConfigHeight, m_iCaptureConfigWidth, getNegotiatedFinalWidth(), getNegotiatedFinalHeight(), millisThisRoundTook, m_fFpsSinceBeginningOfTime, 1.0 * 1000 / millisThisRoundTook,
    /* average */ 1.0 * 1000 * m_iFrameNumber / sumMillisTook, 1.0 * 1000 / fastestRoundMillis, GetFps(), countMissed);
#ifdef _DEBUG // probably not worth it but we do hit this a lot...hmm...
  LocalOutput(out);
  set_config_string_setting(L"frame_stats", out);
#endif
  return S_OK;
}

float COgamaCapturePinDesktop::GetFps() {
  return (float)(UNITS / m_rtFrameLength);
}

void COgamaCapturePinDesktop::reReadCurrentPosition(int isReRead) {
  __int64 start = StartCounter();

  // assume 0 means not set...negative ignore :)
  // TODO no overflows, that's a bad value too... they cause a crash, I think! [position youtube too far bottom right, track it...]
  int old_x = m_rScreen.left;
  int old_y = m_rScreen.top;

  int config_start_x = read_config_setting(TEXT("start_x"), m_rScreen.left);
  m_rScreen.left = config_start_x;

  // is there a better way to do this registry stuff?
  int config_start_y = read_config_setting(TEXT("start_y"), m_rScreen.top);
  m_rScreen.top = config_start_y;
  if (old_x != m_rScreen.left || old_y != m_rScreen.top) {
    if (isReRead) {
      m_rScreen.right = m_rScreen.left + m_iCaptureConfigWidth;
      m_rScreen.bottom = m_rScreen.top + m_iCaptureConfigHeight;
    }
  }

  if (show_performance) {
    wchar_t out[1000];
    swprintf(out, 1000, L"new screen pos from reg: %d %d\n", config_start_x, config_start_y);
    LocalOutput("[re]readCurrentPosition (including swprintf call) took %.02fms", GetCounterSinceStartMillis(start)); // takes 0.42ms (2000 fps)
    LocalOutput(out);
  }
}

COgamaCapturePinDesktop::~COgamaCapturePinDesktop()
{
  // They *should* call this...VLC does at least, correctly.

  // Release the device context stuff
  ::ReleaseDC(NULL, hScrDc);
  ::DeleteDC(hScrDc);
  DbgLog((LOG_TRACE, 3, TEXT("Total no. Frames written %d"), m_iFrameNumber));
  set_config_string_setting(L"last_run_performance", out);

  if (hRawBitmap)
    DeleteObject(hRawBitmap); // don't need those bytes anymore -- I think we are supposed to delete just this and not hOldBitmap

  if (pOldData) {
    free(pOldData);
    pOldData = NULL;
  }
}

void COgamaCapturePinDesktop::CopyScreenToDataBlock(HDC hScrDC, BYTE *pData, BITMAPINFO *pHeader, IMediaSample *pSample)
{
  HDC         hMemDC;         // screen DC and memory DC
  HBITMAP     hOldBitmap;    // handles to device-dependent bitmaps
  int         nX, nY;       // coordinates of rectangle to grab
  int         iFinalStretchHeight = getNegotiatedFinalHeight();
  int         iFinalStretchWidth = getNegotiatedFinalWidth();

  ASSERT(!IsRectEmpty(&m_rScreen)); // that would be unexpected
  // create a DC for the screen and create
  // a memory DC compatible to screen DC   

  hMemDC = CreateCompatibleDC(hScrDC); //  0.02ms Anything else to reuse, this one's pretty fast...?

  // determine points of where to grab from it, though I think we control these with m_rScreen
  nX = m_rScreen.left;
  nY = m_rScreen.top;

  // sanity checks--except we don't want it apparently, to allow upstream to dynamically change the size? Can it do that?
  ASSERT(m_rScreen.bottom - m_rScreen.top == iFinalStretchHeight);
  ASSERT(m_rScreen.right - m_rScreen.left == iFinalStretchWidth);

  // select new bitmap into memory DC
  hOldBitmap = (HBITMAP)SelectObject(hMemDC, hRawBitmap);

  doJustBitBltOrScaling(hMemDC, m_iCaptureConfigWidth, m_iCaptureConfigHeight, iFinalStretchWidth, iFinalStretchHeight, hScrDC, nX, nY);

  // AddMouse(hMemDC, &m_rScreen, hScrDC, m_iHwndToTrack);

  // select old bitmap back into memory DC and get handle to
  // bitmap of the capture...whatever this even means...	
  HBITMAP hRawBitmap2 = (HBITMAP)SelectObject(hMemDC, hOldBitmap);

  BITMAPINFO tweakableHeader;
  memcpy(&tweakableHeader, pHeader, sizeof(BITMAPINFO));

  if (m_bConvertToI420) {
    tweakableHeader.bmiHeader.biBitCount = 32;
    tweakableHeader.bmiHeader.biCompression = BI_RGB;
    tweakableHeader.bmiHeader.biHeight = -tweakableHeader.bmiHeader.biHeight; // prevent upside down conversion from i420...
    tweakableHeader.bmiHeader.biSizeImage = GetBitmapSize(&tweakableHeader.bmiHeader);
  }

  if (m_bConvertToI420) {
    // copy it to a temporary buffer first
    doDIBits(hScrDC, hRawBitmap2, iFinalStretchHeight, pOldData, &tweakableHeader);
    // memcpy(/* dest */ pOldData, pData, pSample->GetSize()); // 12.8ms for 1920x1080 desktop
    // TODO smarter conversion/memcpy's here [?] we could combine scaling with rgb32_to_i420 for instance...
    // or maybe we should integrate with libswscale here so they can request whatever they want LOL. (might be a higher quality i420 conversion...)
    // now convert it to i420 into the "real" buffer
    rgb32_to_i420(iFinalStretchWidth, iFinalStretchHeight, (const char *)pOldData, (char *)pData);// took 36.8ms for 1920x1080 desktop	
  }
  else {
    doDIBits(hScrDC, hRawBitmap2, iFinalStretchHeight, pData, &tweakableHeader);
  }

  // clean up
  DeleteDC(hMemDC);
}

void COgamaCapturePinDesktop::doJustBitBltOrScaling(HDC hMemDC, int nWidth, int nHeight, int iFinalWidth, int iFinalHeight, HDC hScrDC, int nX, int nY) {
  __int64 start = StartCounter();

  boolean notNeedStretching = (iFinalWidth == nWidth) && (iFinalHeight == nHeight);

  //	if(m_iHwndToTrack != NULL)
  //		ASSERT(notNeedStretching); // we don't support HWND plus scaling...hmm... LODO move assertion LODO implement this (low prio since they probably are just needing that window, not with scaling too [?])

  int captureType = SRCCOPY;
  if (m_bUseCaptureBlt)
    captureType = captureType | CAPTUREBLT; // CAPTUREBLT here [last param] is for layered (transparent) windows in non-aero I guess (which happens to include the mouse, but we do that elsewhere)

  if (notNeedStretching) {

    //if(m_iHwndToTrack != NULL) {
    //     // make sure we only capture 'not too much' i.e. not past the border of this HWND, for the case of Aero being turned off, it shows other windows that we don't want
    //  // a bit confusing...
    //     RECT p;
    //  GetClientRect(m_iHwndToTrack, &p); // 0.005 ms
    //     //GetRectOfWindowIncludingAero(m_iHwndToTrack, &p); // 0.05 ms
    //  nWidth = min(p.right-p.left, nWidth);
    //  nHeight = min(p.bottom-p.top, nHeight);
    //   }

    // Bit block transfer from screen our compatible memory DC.	Apparently this is faster than stretching.
    BitBlt(hMemDC, 0, 0, nWidth, nHeight, hScrDC, nX, nY, captureType);
    // 9.3 ms 1920x1080 -> 1920x1080 (100 fps) (11 ms? 14? random?)
  }
  else {
    if (m_iStretchMode == 0)
    {
      // low quality scaling -- looks terrible
      SetStretchBltMode(hMemDC, COLORONCOLOR); // the SetStretchBltMode call itself takes 0.003ms
      // COLORONCOLOR took 92ms for 1920x1080 -> 1000x1000, 69ms/80ms for 1920x1080 -> 500x500 aero
      // 20 ms 1920x1080 -> 500x500 without aero
      // LODO can we get better results with good speed? it is sooo ugly.
    }
    else
    {
      SetStretchBltMode(hMemDC, HALFTONE);
      // high quality stretching
      // HALFTONE took 160ms for 1920x1080 -> 1000x1000, 107ms/120ms for 1920x1080 -> 1000x1000
      // 50 ms 1920x1080 -> 500x500 without aero
      SetBrushOrgEx(hMemDC, 0, 0, 0); // MSDN says I should call this after using HALFTONE
    }
    StretchBlt(hMemDC, 0, 0, iFinalWidth, iFinalHeight, hScrDC, nX, nY, nWidth, nHeight, captureType);
  }

  if (show_performance)
    LocalOutput("%s took %.02f ms", notNeedStretching ? "bitblt" : "stretchblt", GetCounterSinceStartMillis(start));
}

int COgamaCapturePinDesktop::getNegotiatedFinalWidth() {
  int iImageWidth = m_rScreen.right - m_rScreen.left;
  ASSERT(iImageWidth > 0);
  return iImageWidth;
}

int COgamaCapturePinDesktop::getNegotiatedFinalHeight() {
  // might be smaller than the "getCaptureDesiredFinalWidth" if they tell us to give them an even smaller setting...
  int iImageHeight = (int)m_rScreen.bottom - m_rScreen.top;
  ASSERT(iImageHeight > 0);
  return iImageHeight;
}

int COgamaCapturePinDesktop::getCaptureDesiredFinalWidth() {
  if (m_iStretchToThisConfigWidth > 0) {
    return m_iStretchToThisConfigWidth;
  }
  else {
    return m_iCaptureConfigWidth; // full/config setting, static
  }
}

int COgamaCapturePinDesktop::getCaptureDesiredFinalHeight(){
  if (m_iStretchToThisConfigHeight > 0) {
    return m_iStretchToThisConfigHeight;
  }
  else {
    return m_iCaptureConfigHeight; // defaults to full/config static
  }
}

void COgamaCapturePinDesktop::doDIBits(HDC hScrDC, HBITMAP hRawBitmap, int nHeightScanLines, BYTE *pData, BITMAPINFO *pHeader) {
  __int64 start = StartCounter();

  // Copy the bitmap data into the provided BYTE buffer, in the right format I guess.
  GetDIBits(hScrDC, hRawBitmap, 0, nHeightScanLines, pData, pHeader, DIB_RGB_COLORS);  // just copies raw bits to pData, I guess, from an HBITMAP handle. "like" GetObject, but also does conversions [?]

  if (show_performance)
    LocalOutput("doDiBits took %.02fms", GetCounterSinceStartMillis(start)); // took 1.1/3.8ms total, so this brings us down to 80fps compared to max 251...but for larger things might make more difference...
}


//
// DecideBufferSize
//
// This will always be called after the format has been sucessfully
// negotiated (this is negotiatebuffersize). So we have a look at m_mt to see what size image we agreed.
// Then we can ask for buffers of the correct size to contain them.
//
HRESULT COgamaCapturePinDesktop::DecideBufferSize(IMemAllocator *pAlloc,
  ALLOCATOR_PROPERTIES *pProperties)
{
  CheckPointer(pAlloc, E_POINTER);
  CheckPointer(pProperties, E_POINTER);

  CAutoLock cAutoLock(m_pFilter->pStateLock());
  HRESULT hr = NOERROR;

  VIDEOINFO *pvi = (VIDEOINFO *)m_mt.Format();
  BITMAPINFOHEADER header = pvi->bmiHeader;
  ASSERT(header.biPlanes == 1); // sanity check
  // ASSERT(header.biCompression == 0); // meaning "none" sanity check, unless we are allowing for BI_BITFIELDS [?]
  // now try to avoid this crash [XP, VLC 1.1.11]: vlc -vvv dshow:// :dshow-vdev="OgamaCapture" :dshow-adev --sout  "#transcode{venc=theora,vcodec=theo,vb=512,scale=0.7,acodec=vorb,ab=128,channels=2,samplerate=44100,audio-sync}:standard{access=file,mux=ogg,dst=test.ogv}" with 10x10 or 1000x1000
  // LODO check if biClrUsed is passed in right for 16 bit [I'd guess it is...]
  // pProperties->cbBuffer = pvi->bmiHeader.biSizeImage; // too small. Apparently *way* too small.

  int bytesPerLine;
  // there may be a windows method that would do this for us...GetBitmapSize(&header); but might be too small for VLC? LODO try it :)
  // some pasted code...
  int bytesPerPixel = (header.biBitCount / 8);
  if (m_bConvertToI420) {
    bytesPerPixel = 32 / 8; // we convert from a 32 bit to i420, so need more space in this case
  }

  bytesPerLine = header.biWidth * bytesPerPixel;
  /* round up to a dword boundary */
  if (bytesPerLine & 0x0003)
  {
    bytesPerLine |= 0x0003;
    ++bytesPerLine;
  }

  ASSERT(header.biHeight > 0); // sanity check
  ASSERT(header.biWidth > 0); // sanity check
  // NB that we are adding in space for a final "pixel array" (http://en.wikipedia.org/wiki/BMP_file_format#DIB_Header_.28Bitmap_Information_Header.29) even though we typically don't need it, this seems to fix the segfaults
  // maybe somehow down the line some VLC thing thinks it might be there...weirder than weird.. LODO debug it LOL.
  int bitmapSize = 14 + header.biSize + (long)(bytesPerLine)*(header.biHeight) + bytesPerLine*header.biHeight;
  pProperties->cbBuffer = bitmapSize;
  //pProperties->cbBuffer = max(pProperties->cbBuffer, m_mt.GetSampleSize()); // didn't help anything
  if (m_bConvertToI420) {
    pProperties->cbBuffer = header.biHeight * header.biWidth * 3 / 2; // necessary to prevent an "out of memory" error for FMLE. Yikes. Oh wow yikes.
  }

  pProperties->cBuffers = 1; // 2 here doesn't seem to help the crashes...

  // Ask the allocator to reserve us some sample memory. NOTE: the function
  // can succeed (return NOERROR) but still not have allocated the
  // memory that we requested, so we must check we got whatever we wanted.
  ALLOCATOR_PROPERTIES Actual;
  hr = pAlloc->SetProperties(pProperties, &Actual);
  if (FAILED(hr))
  {
    return hr;
  }

  // Is this allocator unsuitable?
  if (Actual.cbBuffer < pProperties->cbBuffer)
  {
    return E_FAIL;
  }

  // now some "once per run" setups

  // LODO reset aer with each run...somehow...somehow...Stop method or something...
  OSVERSIONINFOEX version;
  ZeroMemory(&version, sizeof(OSVERSIONINFOEX));
  version.dwOSVersionInfoSize = sizeof(OSVERSIONINFOEX);
  GetVersionEx((LPOSVERSIONINFO)&version);
  if (version.dwMajorVersion >= 6) { // meaning vista +
    if (read_config_setting(TEXT("disable_aero_for_vista_plus_if_1"), 0) == 1)
      turnAeroOn(false);
    else
      turnAeroOn(true);
  }

  if (pOldData) {
    free(pOldData);
    pOldData = NULL;
  }
  pOldData = (BYTE *)malloc(max(pProperties->cbBuffer*pProperties->cBuffers, bitmapSize)); // we convert from a 32 bit to i420, so need more space, hence max
  memset(pOldData, 0, pProperties->cbBuffer*pProperties->cBuffers); // reset it just in case :P	

  // create a bitmap compatible with the screen DC
  if (hRawBitmap)
    DeleteObject(hRawBitmap);
  hRawBitmap = CreateCompatibleBitmap(hScrDc, getNegotiatedFinalWidth(), getNegotiatedFinalHeight());

  previousFrameEndTime = 0; // reset
  m_iFrameNumber = 0;

  return NOERROR;
} // DecideBufferSize


HRESULT COgamaCapturePinDesktop::OnThreadCreate() {
  LocalOutput("PSD on thread create");
  previousFrameEndTime = 0; // reset <sigh> dunno if this helps FME which sometimes had inconsistencies, or not
  m_iFrameNumber = 0;
  return S_OK;
}