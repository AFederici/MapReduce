#ifndef MESSAGESTYPES_H
#define MESSAGESTYPES_H

extern const char *messageTypes[];

enum MessageType {
	ACK,
	JOIN, //used in joinSystem to join
	LEADERHEARTBEAT, //send with heartbeat that this node is the leader
	LEADERPENDING, //send instead of heartbeat if its currently a blackout
	HEARTBEAT, //non-leader heartbeat
	SWREQ, //tell other nodes to switch mode
	SWRESP, //ack the switch request
	JOINRESPONSE, //introducer sends memberhsip list
	JOINREJECT, //if there is a hash collision introducer rejects the join (try new port?)
	ELECTION, //used to ELECT self as leader
	ELECTIONACK, //after this message makes it back to proposed leader election is over
	PUT, //metadata followed by the actual data, add PUTACK into msg queue
	PUTACK, //add file to localFileList and send an ACK to sender of PUT
	LEADERACK,
	DNS, //sent to leader on PUT request, update pending requests and send out DNSANS
	DNSANS, //PUT file to whatever node got sent by DNS
	DNSGET, //sent to leader on GET request, sends out REPLICATEGET to selected node with file
	DELETE, //leader handles deletion and disseminates info
	GETNULL, //sent if user tries to get a file from leader and it has 0 copies in the system
	REREPLICATE,
	REREPLICATEGET,
	MAPLESTART, //send to master to initiate maple phase
	JUICESTART, //send to master to initiate juice phase
	MAPLEACK,
	CHUNK, //send to nodes so they have information about what kind of get request to send
	CHUNKACK, //after append ack received, send this back to master to know when things are Done
};

enum PayloadType {
	REGULAR=97, //start of actual message (membershipList)
	FILEPAIR, //start of filelist
	FILENAME, //start of filename
	FILEPOSITIONS, //start of comma seperated file positions string
	FILESIZE
};

enum LogType {
	JOINGROUP,
	UPDATE,
	FAIL,
	LEAVE,
	REMOVE,
	GOSSIPTO,
	GOSSIPFROM,
	BANDWIDTH,
	MEMBERS
};

#endif //MESSAGESTYPES_H
