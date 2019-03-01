#include "ipcam_gst.h"
#include <string.h>


int main(int argc, char *argv[]) {

    IPCamData ipcamdata;
    DummyCamData dummyvideodata;
    CustomData data;
    GstStateChangeReturn ret;
    gboolean pipelinecreated = FALSE;
    gboolean pipelinebuild = FALSE;

    // Assign the allocated memory of the inner structs to the CustomData struct.
    data.IPCamData1 = &ipcamdata;
    data.IPCamData2 = &dummyvideodata;

    /* Initialize GStreamer */
    gst_init (&argc, &argv);
    /* Read the camera IP from the first argument*/

    /* Create the pipeline and all its elements */
    pipelinecreated = create_pipeline_elements(&data);
    if(!pipelinecreated){
        g_printerr ("The pipeline or the elements could not be created.\n");
        return -1;
    }

    /* Build the pipeline and link all the static pads of the elements */
    pipelinebuild = build_pipeline(&data);
    if(!pipelinebuild){
        g_printerr ("Building the pipeline has failed.\n");
        return -1;
    }

    /* Set the elements properties in the pipeline */
    set_properties(&data);
    /* Connect to the pad-added signal for the rtsp source and the decodebin */
    pad_added_signal_connections(&data);

    /* Start playing */
    ret = gst_element_set_state (data.pipeline, GST_STATE_PLAYING);
    if (ret == GST_STATE_CHANGE_FAILURE) {
        g_printerr ("Unable to set the pipeline to the playing state.\n");
        gst_object_unref (data.pipeline);
        return -1;
    }

    /* Listen to the bus for messages of ERROR, EOS, STATUS_CHANGED */
    bus_listening(data.pipeline);

    /* Free resources */
    gst_element_set_state (data.pipeline, GST_STATE_NULL);
    gst_object_unref (data.pipeline);
    return 0;
}

/**
 * This function will be called by the pad-added signal for the rtspsrc and the decodebin element.
 * It dynamically connects to the newly created pad of the src element to the sink pad of the data
 * element. This function declaration follows the template of the gstreamer pad-added signal handler
 * proposal.
 *
 * Arg#1: GstElement *src - The source element from the pipeline. This element dynamically allocates the
 * new pad.
 *
 * Arg#2: GstPad *new_pad - The newly created pad.
 *
 * Arg#3: GstElement *data - The destination element with the sink pad. A connection will be established
 * from the new_pad to the sink pad. From the src element to the data element.
 * RetVal#1: void
 * */
static void pad_added_handler (GstElement *src, GstPad *new_pad, GstElement *data) {
    GstPad *sink_pad;
    //g_print ("The pad-added handler from %s\n",gst_element_get_name(data));
    if (strcmp(gst_element_get_name(data),"ipcam1_text") == 0)
        sink_pad = gst_element_get_static_pad (data, "video_sink");
    else
        sink_pad = gst_element_get_static_pad (data, "sink");

    GstPadLinkReturn ret;
    GstCaps *new_pad_caps = NULL;
    GstStructure *new_pad_struct = NULL;
    const gchar *new_pad_type = NULL;

    g_print ("Received new pad '%s' from '%s':\n", GST_PAD_NAME (new_pad), GST_ELEMENT_NAME (src));

    /* If our converter is already linked, we have nothing to do here */
    if (gst_pad_is_linked (sink_pad)) {
        g_print ("We are already linked. Ignoring.\n");

        /* Unreference the new pad's caps */
        gst_caps_unref (new_pad_caps);
        /* Unreference the sink pad */
        gst_object_unref (sink_pad);
        return;
    }

    /* Check the new pad's type */
    new_pad_caps = gst_pad_get_current_caps (new_pad);
    new_pad_struct = gst_caps_get_structure (new_pad_caps, 0);
    new_pad_type = gst_structure_get_name (new_pad_struct);
    if (g_str_has_prefix (new_pad_type, "application/x-rtp") || g_str_has_prefix (new_pad_type, "video/x-raw")) {

        /* Attempt to link */
        ret = gst_pad_link (new_pad, sink_pad);
        if (GST_PAD_LINK_FAILED (ret)) {
            g_print ("Type is '%s' but link failed.\n", new_pad_type);
        } else {
            g_print ("Link succeeded (type '%s').\n", new_pad_type);
        }
        return;
    }
    else {
        g_print ("It has type '%s' which is not in our range of interest. Ignoring.\n", new_pad_type);

        /* Unreference the new pad's caps, if we got them */
        if (new_pad_caps != NULL)
            gst_caps_unref (new_pad_caps);
        /* Unreference the sink pad */
        gst_object_unref (sink_pad);
    }

}

/**
 * This is a function to request pads from a Gst Element with "on request" pads.
 *
 * Arg#1: GstElement *ElemOnRequestPads
 * Arg#2: const gchar *pad_template -  Pad template as a string. Check gst-inspect(ie "sink_%u")
 * RetVal#1: void
 * */
GstPad *OnRequestReturnPad(GstElement *ElemOnRequestPads, const gchar *pad_template){

    GstPad *pad;
    gchar *name;

    pad = gst_element_get_request_pad (ElemOnRequestPads, pad_template);
    name = gst_pad_get_name (pad);

    g_print ("OnRequest Return Pad: A new pad %s was created from %s\n", name, gst_element_get_name(ElemOnRequestPads));
    g_free (name);
    return pad;
}

/**
 * Group the connections to the pad-added signals.
 *
 * Arg#1: CustomData *data - Pointer to the CustomData structure containing the elements of the pipeline
 * RetVal#1: void
 * */
void pad_added_signal_connections(CustomData *data){

    /* Connect to the pad-added signal for the rtsp source and the decodebin */
    g_signal_connect (data->IPCamData1->IPCamRTSPsrc, "pad-added", G_CALLBACK (pad_added_handler), data->IPCamData1->rtph264depayload);
    g_signal_connect (data->IPCamData1->decbin, "pad-added", G_CALLBACK (pad_added_handler), data->IPCamData1->text);
}


/**
 * This functions creates the pipeline elements, the pipeline and
 * returns true if the elements have been created otherwise it returns false.
 *
 * Arg#1: CustomData *data - Pointer to the CustomData structure containing the elements of the pipeline
 * RetVal#1: gboolean - TRUE if all the elements have been created. Otherwise returns FALSE.
 * */
gboolean create_pipeline_elements(CustomData *data){

    /* Create the pipeline elements for IPCam 1 */
    data->IPCamData1->IPCamRTSPsrc = gst_element_factory_make ("rtspsrc", "ipcam1_source");
    data->IPCamData1->rtph264depayload = gst_element_factory_make ("rtph264depay", "rtph264depayload");
    data->IPCamData1->decbin = gst_element_factory_make ("decodebin", "decbin");
    data->IPCamData1->text = gst_element_factory_make ("textoverlay", "ipcam1_text");

    /* Create the pipeline elements for dummy video input (simulate IPCam2) */
    data->IPCamData2->videotestsource = gst_element_factory_make ("videotestsrc", "vtst_source");
    data->IPCamData2->text = gst_element_factory_make ("textoverlay", "vtst_text");

    /* Create the pipeline elements after the videomixer */
    data->videomix = gst_element_factory_make ("videomixer", "videomix");
    data->videoconv = gst_element_factory_make ("videoconvert", "videoconv");
    data->videosink = gst_element_factory_make ("autovideosink", "videosink");

    /**
     * Create an empty pipeline to include all the IP camera bins.
     * Create the IP camera bin to include all the IP camera elements.
     * Create the dummy video bin to include all the dummy video elements.
     * */
    data->pipeline = gst_pipeline_new ("the_pipeline");
    data->IPCamData1->ipcambin = gst_bin_new ("ipcambin1");
    data->IPCamData2->dummyvideobin = gst_bin_new ("dummyvideobin");

    /* Check if the elements could be created */
    if (!data->IPCamData1->IPCamRTSPsrc ||
        !data->IPCamData1->rtph264depayload ||
        !data->IPCamData1->decbin ||
        !data->IPCamData1->text ||
        !data->IPCamData2->videotestsource ||
        !data->IPCamData2->text ||
        !data->videomix ||
        !data->videoconv ||
        !data->videosink ||
        !data->pipeline) {

        g_printerr ("Not all elements could be created.\n");
        return FALSE;
    }
    return TRUE;

}

/**
 * Build the pipeline. Note that we are NOT linking the source and the decodebin at this
 * point. We will do it later with the pad-added signal.
 *
 * Arg#1: CustomData *data - Pointer to the CustomData structure containing the elements of the pipeline
 * RetVal#1: gboolean - TRUE if all the elements have been created. Otherwise returns FALSE.
 * */
gboolean build_pipeline(CustomData *data){

    GstPad *vmix_sinkpad1, *vmix_sinkpad2;

    // Add the IP camera elements in the IP camera bin.
    gst_bin_add_many(GST_BIN(data->IPCamData1->ipcambin),
                     data->IPCamData1->IPCamRTSPsrc,
                     data->IPCamData1->rtph264depayload,
                     data->IPCamData1->decbin,
                     data->IPCamData1->text,
                     NULL);
    // Add the dummy video elements in the dummy video bin.
    gst_bin_add_many(GST_BIN(data->IPCamData2->dummyvideobin),
                     data->IPCamData2->videotestsource,
                     data->IPCamData2->text,
                     NULL);

    // Add all the extra elements and bins into the overall pipeline.
    gst_bin_add_many (GST_BIN (data->pipeline),
            data->IPCamData1->ipcambin,
            data->IPCamData2->dummyvideobin,
            data->videomix,
            data->videoconv,
            data->videosink,
            NULL);

//    gst_bin_add_many (GST_BIN (data->pipeline),
//                      data->IPCamData1->IPCamRTSPsrc,
//                      data->IPCamData1->rtph264depayload,
//                      data->IPCamData1->decbin,
//                      data->IPCamData1->text,
//                      data->IPCamData2->videotestsource,
//                      data->IPCamData2->text,
//                      data->videomix,
//                      data->videoconv,
//                      data->videosink,
//                      NULL);
    /**
     * On Request Pads:
     * The sink pads (sink_%u) of the videomixer are on request pads. Manually linked.
     * textoverlay - videomixer (for each channel)
     *
     * Dynamic pad-added:
     * rtspsrc (dynamic src pad) - (static sink pad)rtph264depay
     * decodebin (dynamic src) - (static video_sink pad)textoverlay
     * videotestsrc (src)
     *
     * */

    // Request the two sink pads from the videomixer
    vmix_sinkpad1 = OnRequestReturnPad(data->videomix, "sink_%u");
    vmix_sinkpad2 = OnRequestReturnPad(data->videomix, "sink_%u");

    // Link all the elements with static pads. Dynamic pad linking will be handled later with the pad-added signal.
    if (!gst_element_link(data->IPCamData1->rtph264depayload, data->IPCamData1->decbin) ||
        !gst_element_link_pads(data->IPCamData1->text, "src", data->videomix, gst_pad_get_name(vmix_sinkpad1)) ||
        !gst_element_link_pads(data->IPCamData2->videotestsource, "src",data->IPCamData2->text, "video_sink") ||
        !gst_element_link_pads(data->IPCamData2->text, "src", data->videomix, gst_pad_get_name(vmix_sinkpad2)) ||
        !gst_element_link_many(data->videomix, data->videoconv, data->videosink, NULL))
    {

        g_printerr ("Elements could not be linked.\n");

        gst_object_unref (data->pipeline);
        gst_object_unref(GST_OBJECT (vmix_sinkpad1));
        gst_object_unref(GST_OBJECT (vmix_sinkpad2));

        return FALSE;
    }

    return TRUE;
}

/**
 * Set properties of the elements in the pipeline.
 *
 * Arg#1: CustomData *data - Pointer to the CustomData structure containing the elements of the pipeline
 * RetVal#1: void
 * */
void set_properties(CustomData *data){

    /* Set the rtspsrc element properties (URI to play, latency, etc) */
    g_object_set (G_OBJECT(data->IPCamData1->IPCamRTSPsrc), "location", "rtsp://itiuser:itiuser@10.8.1.101:554/videoMain", NULL);
    g_object_set (G_OBJECT(data->IPCamData1->IPCamRTSPsrc), "latency", 0, NULL);

    /* Set the textoverlay for IPCam bin element properties (text, font, background, position) */
    g_object_set (G_OBJECT(data->IPCamData1->text), "valignment", 1, NULL); //bottom - 1
    g_object_set (G_OBJECT(data->IPCamData1->text), "halignment", 0, NULL); //left - 0
    g_object_set (G_OBJECT(data->IPCamData1->text), "text", "IPCAM#1", NULL);
    g_object_set (G_OBJECT(data->IPCamData1->text), "shaded-background", TRUE, NULL);

    /* Set the textoverlay for the dummy video bin element properties (text, font, background, position) */
    g_object_set (G_OBJECT(data->IPCamData2->text), "valignment", 1, NULL); //bottom - 1
    g_object_set (G_OBJECT(data->IPCamData2->text), "halignment", 0, NULL); //left - 0
    g_object_set (G_OBJECT(data->IPCamData2->text), "text", "TESTCAM#2", NULL);
    g_object_set (G_OBJECT(data->IPCamData2->text), "shaded-background", TRUE, NULL);

    /* Set the videomixer properties to split the screen */

}

/**
 * Listen to the bus of the pipeline for messages of type ERROR, EOS, STATE CHANGED.
 *
 * Arg#1: GstElement *apipeline - The argument must be a pipeline in order ot return the bus.
 * RetVal#1: void
 * */
void bus_listening(GstElement *apipeline){
    GstBus *bus;
    GstMessage *msg;
    gboolean terminate = FALSE;

    bus = gst_element_get_bus (apipeline);
    do {
        msg = gst_bus_timed_pop_filtered (bus, GST_CLOCK_TIME_NONE,
                                          GST_MESSAGE_STATE_CHANGED | GST_MESSAGE_ERROR | GST_MESSAGE_EOS);

        /* Parse message */
        if (msg != NULL) {
            GError *err;
            gchar *debug_info;

            switch (GST_MESSAGE_TYPE (msg)) {
                case GST_MESSAGE_ERROR:
                    gst_message_parse_error (msg, &err, &debug_info);
                    g_printerr ("Error received from element %s: %s\n", GST_OBJECT_NAME (msg->src), err->message);
                    g_printerr ("Debugging information: %s\n", debug_info ? debug_info : "none");
                    g_clear_error (&err);
                    g_free (debug_info);
                    terminate = TRUE;
                    break;
                case GST_MESSAGE_EOS:
                    g_print ("End-Of-Stream reached.\n");
                    terminate = TRUE;
                    break;
                case GST_MESSAGE_STATE_CHANGED:
                    /* We are only interested in state-changed messages from the pipeline */
                    if (GST_MESSAGE_SRC (msg) == GST_OBJECT (apipeline)) {
                        GstState old_state, new_state, pending_state;
                        gst_message_parse_state_changed (msg, &old_state, &new_state, &pending_state);
                        g_print ("Pipeline state changed from %s to %s:\n",
                                 gst_element_state_get_name (old_state), gst_element_state_get_name (new_state));
                    }
                    break;
                default:
                    /* We should not reach here */
                    g_printerr ("Unexpected message received.\n");
                    break;
            }
            gst_message_unref (msg);
        }
    } while (!terminate);

    /* Free resources */
    gst_object_unref (bus);
}