15441 Project 2 final submit
by Shuyang Wang(shuyangw), Qi Liu(qiliu)

*******************************************************************************
How to run:
For each node, you should run webserver and routing daemon server.

1. run flask server: 
under starter/, 
./webserver.py <local-port>,

for example: ./webserver.py 5001

Note, for each node, the index.html under static should be modified before
the flask server start. The port number in the index.html should be in
consistant with the local port of rd running on the node. 

2. run routing daemon server: 
under src/, 
make to build, then input:
./routed <nodeid> <config file> <file list> <adv cycle time> 
  <neighbor timeoout> <retran timeout> <LSA timeout>

for example: ./routd 1 ../somefolder/node1.conf ../somefolder/node1.files 30 120 3 120

*******************************************************************************
Basic Ideas about Design:

This is a distributed webserver with many nodes, on each node, there is a 
webserver (flask) and a routing daemon server (rd) run on it.

1. About Routing
As specified in project handout, we implement a simple dijkstra algorithm to 
help every node to find the shortest path to any node in this topology. 
Each node will get topology information from LSAs communicated among all nodes,
and calculate and form the routing table. When the node get new LSA, it will 
recalculate the routing to form a new routing table. With routing table, the 
node can know how to reach a specific node. We implement the binary search for
routing table to improve the searching performance.

2. About File Locating
From the routing table described above, we will go a further step to form a 
'forwarding table'. For each file we know, will record all the nodes who have 
this file. Then we will compare the cost to those nodes, and select one with 
the least cost as whe destination node for this file. We record the next-hop 
to the destination in our fowarding table. So, for each file, we will know 
which neighbor should be asked to get it.

When the rd server get a request (from local webserver) for a specific file, 
we will search it in our 'forwarding table'. If we can find it, we will send 
the request to the appropriate node (the next-hop). Then the next-hop will 
transfer the request to next-next-hop, until we reach the destination node.

3. About Timeout
The 'select' function provided us with a timer (the last argument of select 
function). We use it as our time out timer. For each selection round, we 
try to check is there any timeout happen. However, this means too many hash 
table traversing. To improve the performance, we will not check timeout in 
every select round. 

We will set the last argument of select function to 2sec to make it more 
precise. We assume that the <retran time> provided by the arguments is the
smallest one, and could be as less as 2 seconds.

So we can traverse hash table every 2 seconds to find out any 
timeout, this will reduce lots of calculation cost and improve the 
performance. On the other hand, this will make the time not that precise, 
but we think this is OK.

4. Longest Prefix Match
We implement a prefix tree to match each objects with the longest prefix.  
The object will be leveled according to '/'.
For example, a string "/a/b/e" will be divided to 4 levels.
"/", "a/", "b/", and "e".

If node 1 claim it has /a/, node 2 claim it has /a/b/, node 3 claim it has
/a/b/e, lookup from front end web on node1 for /a/b/e, both rd on node1 and
node2 will match the longest prefix, which is node3.

To do this, a fake obj and file mapping should be added to the config file
"node*.files". It looks like

/a/ FAKE
/a/b/e  /static/sha256

Although, /a/b/e will also be annonced with lsa, only /a/ will be counted in 
matching longest prefix when file lookup.
A test will be provided for longest prefix match.


Notes:
1. After uploading a file on a node, we will write the file information back 
to the node.files file, so after rebooting, the node can still know what 
files it has. This will reduce the unnecessary confusion after rebooting.

2. In your test, if you write a neighbor in the node.conf file, please 
boot it up at least once at the beginning, otherwise please do not write it 
into node.conf file. We assume that the administrator knows the network state
when it boot a router. The neighbor could go down after that, we could handle
this situation.
