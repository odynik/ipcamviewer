//
// Created by odynik on 2/28/2019.
//

#ifndef GSTGTKEXAMPLE_IPCAM_GST_H
#define GSTGTKEXAMPLE_IPCAM_GST_H

#include <gst/gst.h>
/* Structure definition */
typedef struct _CustomData CustomData;
typedef struct _IPCamData IPCamData;
typedef struct _DummyCamData DummyCamData;

/* Functions declaration for the IP camera pipeline */
/* Handler for the pad-added signal */
static void pad_added_handler (GstElement *src, GstPad *pad, GstElement *data);
gboolean create_pipeline_elements(CustomData *data);
gboolean build_pipeline(CustomData *data);
void set_properties(CustomData *data);
void pad_added_signal_connections(CustomData *data);
void bus_listening(GstElement *apipeline);
GstPad *OnRequestReturnPad(GstElement *ElemOnRequestPads, const gchar *type);


/**
 * Structure to contain all the pipeline information, so we can pass it to callbacks
 * The IPCamData contains the sub-pipeline elements for the IP cameras.
 * The DummyCamData contains all the sub-pipeline elements for a dummy videosource.
 * The DummyCamData simulates a video source in order to make videomixer to work.
 * **/
typedef struct _CustomData {
    GstElement *pipeline;
    IPCamData  *IPCamData1;
    DummyCamData *IPCamData2;
    GstElement *videomix;
    GstElement *videoconv;
    GstElement *videosink;
} CustomData;

/* Structure to contain all the pipeline information for an IP Camera.*/
typedef struct _IPCamData {
    GstElement *ipcambin;
    GstElement *IPCamRTSPsrc;
    GstElement *rtph264depayload;
    GstElement *decbin;
    GstElement *text;
} IPCamData;

/* Dummy structure to contain a dummy videosrc to construct the pipeline with the videomixer */
typedef struct _DummyCamData {
    GstElement *dummyvideobin;
    GstElement *videotestsource;
    GstElement *text;
} DummyCamData;

#endif //GSTGTKEXAMPLE_IPCAM_GST_H
