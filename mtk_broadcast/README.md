# Broadcast toolkit

This toolkit provides an API based on link-local multicast and the Trickle Algorithm ([RFC6206](https://tools.ietf.org/html/rfc6206)) to do network wide broadcast of arbitrary data.

Mira Version requirement:  
2.4.0+

## Example

There is a example utilizing and showcasing the broadcast toolkit available in the [Mira examples repository](https://github.com/LumenRadio/mira-examples)

## Broadcast

The toolkit supports multiple instances of broadcast, where each instance is using its own trickle timer.

The API to be used by the application is provided in `mtk_broadcast.h`

The packet structure of broadcasted packets:

| 4 bytes | 4 bytes | max 230 bytes |
|---------|---------|---------------|
|    ID   | Version |      Data     |

## Include the toolkit in your application
To include the toolkit in your application,

- Add the .c files to SOURCE_FILES in your makefile
- Include mtk_broadcast.h in your application
- Provide MTK_BROADCAST_NUM_UNIQUE_BROADCASTS as a compiler argument to set number of unique broadcasts available. (default is 4)