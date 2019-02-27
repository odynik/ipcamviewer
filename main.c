#include <gst/gst.h>

/* Structure to contain all our information, so we can pass it to callbacks */
typedef struct _CustomData {
    GstElement *pipeline;
    GstElement *IPCamRTSPsrc;
    GstElement *rtph264depayload;
    GstElement *decbin;
    GstElement *videoconv;
    GstElement *videosink;
} CustomData;

/* Handler for the pad-added signal */
static void pad_added_handler (GstElement *src, GstPad *pad, GstElement *data);

int main(int argc, char *argv[]) {
    CustomData data;
    GstBus *bus;
    GstMessage *msg;
    GstStateChangeReturn ret;
    gboolean terminate = FALSE;

    /* Initialize GStreamer */
    gst_init (&argc, &argv);
    /* Read the camera IP from the first argument*/

    /* Create the pipeline elements */
    data.IPCamRTSPsrc = gst_element_factory_make ("rtspsrc", "source");
    data.rtph264depayload = gst_element_factory_make ("rtph264depay", "rtph264depayload");
    data.decbin = gst_element_factory_make ("decodebin", "decbin");
    data.videoconv = gst_element_factory_make ("videoconvert", "videoconv");
    data.videosink = gst_element_factory_make ("autovideosink", "videosink");

    /* Create the empty pipeline */
    data.pipeline = gst_pipeline_new ("ipcam_pipeline");

    if (!data.pipeline || !data.IPCamRTSPsrc ||
        !data.rtph264depayload || !data.decbin ||
        !data.videoconv || !data.videosink) {
        g_printerr ("Not all elements could be created.\n");
//        g_printerr("pipeline: %s\n", gst_element_get_name(data.pipeline));
//        g_printerr("IPCamRTSPsrc: %s\n", gst_element_get_name(data.IPCamRTSPsrc));
//        g_printerr("rtph264depayload: %s\n", gst_element_get_name(data.rtph264depayload));
//        g_printerr("decbin: %s\n", gst_element_get_name(data.decbin));
//        g_printerr("videoconv: %s\n", gst_element_get_name(data.videoconv));
//        g_printerr("videosink: %s\n",gst_element_get_name(data.videosink));
        return -1;
    }

    /* Build the pipeline. Note that we are NOT linking the source and the decodebin at this
     * point. We will do it later with the pad-added signal. */
    gst_bin_add_many (GST_BIN (data.pipeline), data.IPCamRTSPsrc, data.rtph264depayload, data.decbin , data.videoconv, data.videosink, NULL);
    if (!gst_element_link(data.rtph264depayload, data.decbin) || !gst_element_link(data.videoconv, data.videosink)) {
        g_printerr ("Elements could not be linked.\n");
        gst_object_unref (data.pipeline);
        return -1;
    }

    /* Set the rtspsrc element properties (URI to play, latency, etc) */
    g_object_set (G_OBJECT(data.IPCamRTSPsrc), "location", "rtsp://itiuser:itiuser@10.8.1.101:554/videoMain", NULL);
    g_object_set (G_OBJECT(data.IPCamRTSPsrc), "latency", 0, NULL);

    /* Connect to the pad-added signal for the rtsp source and the decodebin */
    g_signal_connect (data.IPCamRTSPsrc, "pad-added", G_CALLBACK (pad_added_handler), data.rtph264depayload);
    g_signal_connect (data.decbin, "pad-added", G_CALLBACK (pad_added_handler), data.videoconv);

    /* Start playing */
    ret = gst_element_set_state (data.pipeline, GST_STATE_PLAYING);
    if (ret == GST_STATE_CHANGE_FAILURE) {
        g_printerr ("Unable to set the pipeline to the playing state.\n");
        gst_object_unref (data.pipeline);
        return -1;
    }

    /* Listen to the bus */
    bus = gst_element_get_bus (data.pipeline);
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
                    if (GST_MESSAGE_SRC (msg) == GST_OBJECT (data.pipeline)) {
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
    gst_element_set_state (data.pipeline, GST_STATE_NULL);
    gst_object_unref (data.pipeline);
    return 0;
}

/* This function will be called by the pad-added signal for the rtspsrc element */
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
