/*
So you need to broadcast an alert...
... here's what to do:

1. Copy sendalert.cpp into your bitcoind build directory

2. Decrypt the alert keys
  copy the decrypted file as alertkeys.h into the src/ directory.

3. Modify the alert parameters in sendalert.cpp
  See the comments in the code for what does what.

4. Add sendalert.cpp to the src/Makefile.am so it gets built:

    libbitcoin_server_a_SOURCES = \
      sendalert.cpp \
      ... etc

5. Update init.cpp to launch the send alert thread. 
  Define the thread function as external at the top of init.cpp:

    extern void ThreadSendAlert();

  Add this call at the end of AppInit2:

    threadGroup.create_thread(boost::bind(ThreadSendAlert));

6. build bitcoind, then run it with -printalert or -sendalert
  I usually run it like this:
   ./bitcoind -printtoconsole -sendalert

One minute after starting up the alert will be broadcast. It is then
flooded through the network until the nRelayUntil time, and will be
active until nExpiration OR the alert is cancelled.

If you screw up something, send another alert with nCancel set to cancel
the bad alert.
*/
#include "alert.h"
#include "clientversion.h"
#include "chainparams.h"
#include "init.h"
#include "net.h"
#include "util.h"
#include "utilstrencodings.h"
#include "utiltime.h"

void ThreadSendAlert()
{
    if (!mapArgs.count("-sendalert") && !mapArgs.count("-printalert"))
        return;

    // Wait one minute so we get well connected. If we only need to print
    // but not to broadcast - do this right away.
    if (mapArgs.count("-sendalert"))
        MilliSleep(60*1000);

    //
    // Alerts are relayed around the network until nRelayUntil, flood
    // filling to every node.
    // After the relay time is past, new nodes are told about alerts
    // when they connect to peers, until either nExpiration or
    // the alert is cancelled by a newer alert.
    // Nodes never save alerts to disk, they are in-memory-only.
    //
    CAlert alert;
    alert.nRelayUntil   = GetTime() + 15 * 60;
    alert.nExpiration   = GetTime() + 30 * 60 * 60;
    alert.nID           = 1;  // keep track of alert IDs somewhere
    alert.nCancel       = 0;   // cancels previous messages up to this ID number

    // These versions are protocol versions
    alert.nMinVer       = 70012;
    alert.nMaxVer       = 70012;

    //
    //  1000 for Misc warnings like out of disk space and clock is wrong
    //  2000 for longer invalid proof-of-work chain
    //  Higher numbers mean higher priority
    alert.nPriority     = 5000;
    alert.strComment    = "";
    //alert.strStatusBar  = "URGENT: Upgrade required: see longnetwork.github.io";
    alert.strStatusBar  = "URGENT: CRYPTON Гибнееееет!!!";

    // Set specific client version/versions here. If setSubVer is empty, no filtering on subver is done:
    alert.setSubVer.insert(std::string("/Satoshi:0.12.1(LONGCORE)/"));

    // Sign
    if(!alert.Sign())
    {
        LogPrintf("ThreadSendAlert() : could not sign alert\n");
        return;
    }

    // Test
    CDataStream sBuffer(SER_NETWORK, CLIENT_VERSION);
    sBuffer << alert;
    CAlert alert2;
    sBuffer >> alert2;
    if (!alert2.CheckSignature(Params().AlertKey()))
    {
        printf("ThreadSendAlert() : CheckSignature failed\n");
        return;
    }
    assert(alert2.vchMsg == alert.vchMsg);
    assert(alert2.vchSig == alert.vchSig);
    alert.SetNull();
    printf("\nThreadSendAlert:\n");
    printf("hash=%s\n", alert2.GetHash().ToString().c_str());
    printf("%s", alert2.ToString().c_str());
    printf("vchMsg=%s\n", HexStr(alert2.vchMsg).c_str());
    printf("vchSig=%s\n", HexStr(alert2.vchSig).c_str());

    // Confirm
    if (!mapArgs.count("-sendalert"))
        return;
    while (vNodes.empty() && !ShutdownRequested())
        MilliSleep(500);
    if (ShutdownRequested())
        return;

    // Send
    printf("ThreadSendAlert() : Sending alert\n");
    int nSent = 0;
    {
        LOCK(cs_vNodes);
        BOOST_FOREACH(CNode* pnode, vNodes)
        {
            if (alert2.RelayTo(pnode))
            {
                printf("ThreadSendAlert() : Sent alert to %s\n", pnode->addr.ToString().c_str());
                nSent++;
            }
        }
    }
    printf("ThreadSendAlert() : Alert sent to %d nodes\n", nSent);
}
