#include "ipcam_gst.h"


int main(int argc, char *argv[]) {

    CustomData data;
    GstStateChangeReturn ret;
    gboolean pipelinecreated = FALSE;
    gboolean pipelinebuild = FALSE;

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
    GstPad *sink_pad = gst_element_get_static_pad (data, "sink");
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
 * Group the connections to the pad-added signals.
 *
 * Arg#1: CustomData *data - Pointer to the CustomData structure containing the elements of the pipeline
 * RetVal#1: void
 * */
void pad_added_signal_connections(CustomData *data){

    /* Connect to the pad-added signal for the rtsp source and the decodebin */
    g_signal_connect (data->IPCamRTSPsrc, "pad-added", G_CALLBACK (pad_added_handler), data->rtph264depayload);
    g_signal_connect (data->decbin, "pad-added", G_CALLBACK (pad_added_handler), data->videoconv);
}


/**
 * This functions creates the pipeline elements, the pipeline and
 * returns true if the elements have been created otherwise it returns false.
 *
 * Arg#1: CustomData *data - Pointer to the CustomData structure containing the elements of the pipeline
 * RetVal#1: gboolean - TRUE if all the elements have been created. Otherwise returns FALSE.
 * */
gboolean create_pipeline_elements(CustomData *data){

    /* Create the pipeline elements */
    data->IPCamRTSPsrc = gst_element_factory_make ("rtspsrc", "source");
    data->rtph264depayload = gst_element_factory_make ("rtph264depay", "rtph264depayload");
    data->decbin = gst_element_factory_make ("decodebin", "decbin");
    data->videoconv = gst_element_factory_make ("videoconvert", "videoconv");
    data->videosink = gst_element_factory_make ("autovideosink", "videosink");

    /* Create the empty pipeline */
    data->pipeline = gst_pipeline_new ("ipcam_pipeline");

    /* Check if the elements could be created */
    if (!data->pipeline || !data->IPCamRTSPsrc ||
        !data->rtph264depayload || !data->decbin ||
        !data->videoconv || !data->videosink) {
        g_printerr ("Not all elements could be created.\n");
//        g_printerr("pipeline: %s\n", gst_element_get_name(data.pipeline));
//        g_printerr("IPCamRTSPsrc: %s\n", gst_element_get_name(data.IPCamRTSPsrc));
//        g_printerr("rtph264depayload: %s\n", gst_element_get_name(data.rtph264depayload));
//        g_printerr("decbin: %s\n", gst_element_get_name(data.decbin));
//        g_printerr("videoconv: %s\n", gst_element_get_name(data.videoconv));
//        g_printerr("videosink: %s\n",gst_element_get_name(data.videosink));
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
    // Add all the elements into the bin(pipeline is a bin).
    gst_bin_add_many (GST_BIN (data->pipeline), data->IPCamRTSPsrc, data->rtph264depayload, data->decbin , data->videoconv, data->videosink, NULL);
    // Link all the elements with static pads. Dynamic pad linking will be handled later with the pad-added signal.
    if (!gst_element_link(data->rtph264depayload, data->decbin) || !gst_element_link(data->videoconv, data->videosink)) {
        g_printerr ("Elements could not be linked.\n");
        gst_object_unref (data->pipeline);
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
    g_object_set (G_OBJECT(data->IPCamRTSPsrc), "location", "rtsp://itiuser:itiuser@10.8.1.101:554/videoMain", NULL);
    g_object_set (G_OBJECT(data->IPCamRTSPsrc), "latency", 0, NULL);
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