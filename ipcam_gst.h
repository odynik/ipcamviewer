//
// Created by odynik on 2/28/2019.
//

#ifndef GSTGTKEXAMPLE_IPCAM_GST_H
#define GSTGTKEXAMPLE_IPCAM_GST_H

#include <gst/gst.h>

/* Structure to contain all the pipeline information, so we can pass it to callbacks */
typedef struct _CustomData {
    GstElement *pipeline;
    GstElement *IPCamRTSPsrc;
    GstElement *rtph264depayload;
    GstElement *decbin;
    GstElement *videoconv;
    GstElement *videosink;
} CustomData;

/* Functions declaration for the IP camera pipeline */
/* Handler for the pad-added signal */
static void pad_added_handler (GstElement *src, GstPad *pad, GstElement *data);
gboolean create_pipeline_elements(CustomData *data);
gboolean build_pipeline(CustomData *data);
void set_properties(CustomData *data);
void pad_added_signal_connections(CustomData *data);
void bus_listening(GstElement *apipeline);

#endif //GSTGTKEXAMPLE_IPCAM_GST_H
