# Bulk data collection toolkit

This tool provides a way to send bulk data into sub-packets with
MiraOS v2. It monitors incoming sub-packets and requests missing sub-packets if
needed.

Mira Version requirement:
2.4.0+

## Modules

### bulk_data_collection

Prefix `mtk_bulk_data_collection_`

Processes requests to send bulk data, as well as the reception of sub-packets.

Sender uses this module to send sub-packets in a paced manner, depending on how
it was requested to do so (see module `mtk_bdc_request`).

Receiver uses this module to handle the reception of sub-packets, determine if
sub-packets are missing, and re-request transmission of these missing
sub-packets. Upon receiving a whole bulk data (all its sub-packets), it posts
an event, which the application can use to handle the data.

`mtk_bulk_data_collection_udp_listen_callback` runs at every reception of an UDP packet on
the defined port. This callback then dispatches handling of the content to the
modules described below.

Note: pre-processor define `FAULT_RATE_PERCENT` (default at 0) allows to
simulate packet loss by discarding incoming sub-packets, in order to see the
re-request mechanism at work.

### mtk_bdc_signal

Prefix `mtk_bdcsig_`

This module handles signaling of new available bulk data. Sender uses this
module to notify the network of a new available bulk data. The receiver uses
the module to handle such incoming notifications, and posts an event (with data)
to other processes, if applicable.

### mtk_bdc_request

Prefix `mtk_bdcreq_`

This module handles requests for bulk data. Receiver send requests to the
sender when ready to receive, asking for sub-packets of a bulk data. The
request includes a bit mask, which determines which sub-packets the sender must
send.

Sender uses the module to handle such requests, and posts an event (with data)
to other processes, if applicable.

### mtk_bdc_subpacket

Prefix `mtk_bdcsp_`

This module handles sub-packets, transmission and reception.

## Include the toolkit in your application
To include the toolkit in your application,

- Add the .c files to SOURCE_FILES in your Makefile
- Add the mtk_bulk_data_collection folder path to the CFLAGS make variable.
- Include the relevant header files in your application
