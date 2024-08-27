# pd.link: local network send/receive object for Pd
pd.link simplifies the process of establishing a UDP connection over network to simply using the same send/receive identifier. IP and port configuration are automatically managed for you.
Some common use cases are sending messages between Pd on different computers (or a Raspberry Pi), sending messages between different Pd flavours (pure-data/purr-data/plugdata), or sending messages between different plugdata instances on separate DAW tracks.

# Usage
The one non-flag argument passed in decides the connection name. Two \[pd.link] object with the same connection name will link together.
Pass the -debug flag to debug the connection.
Pass the -local flag to only allow localhost connection (meaning, connections on the same machine)

Messages passed into the inlet will be sent to any other \[pd.link] with the same name.
The outlet will output any messages sent to a \[pd.link] with the same name.
Note that messages sent into the inlet will also go to its own outlet. These messages should have gone to your router and back, meaning they will have a similar amount of latency to messages received remotely. You can use this to keep values more closely synced in time.

# Troubleshooting
- If you have multiple network adapters, it is possible \[pd.link] will pick the wrong one. The only solution as of now is to disable unused network adapters.
- Make sure your firewall (either on your PC or router) is not blocking local UDP connections
  - If you suspect that this is the case, try creating a regular OSC connection. If that also doesn't work, a firewall is likely the problem

# Credits
- Made by Timothy Schoen
- Documentation and maintainance by Alexandre Porres
- Based on [udp-discovery-cpp](https://github.com/truvorskameikin/udp-discovery-cpp) by Dmitrii Tokarev
