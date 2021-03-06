/* startup.c */

#include "../ptpd.h"

PtpClock *ptpClock;

void catch_close(int sig)
{
  char *s;
  
  ptpdShutdown();
  
  switch(sig)
  {
  case SIGINT:
    s = "interrupt";
    break;
    
  case SIGTERM:
    s = "terminate";
    break;
    
  case SIGHUP:
    s = "hangup";
    break;
    
  default:
    s = "?";
  }
  
  NOTIFY("shutdown on %s signal\n", s);
  
  exit(0);
}

void ptpdShutdown()
{
  netShutdown(&ptpClock->netPath);
  
  free(ptpClock->foreign);
  free(ptpClock);
}

PtpClock * ptpdStartup(int argc, char **argv, Integer16 *ret, RunTimeOpts *rtOpts)
{
  int c, fd = -1, nondaemon = 0, noclose = 0;
  
  /* parse command line arguments */
  while( (c = getopt(argc, argv, "?cf:dDxta:w:b:u:l:o:n:y:m:gv:r:s:p:q:i:eh")) != -1 ) {
    switch(c) {
    case '?':
      printf(
"\nUsage:  ptpv2d [OPTION]\n\n"
"Ptpv2d runs on UDP/IP , P2P mode by default\n"
"\n"
"-?                show this page\n"
"\n"
"-c                run in command line (non-daemon) mode\n"
"-f FILE           send output to FILE\n"
"-d                display stats\n"
"-D                display stats in .csv format\n"
"\n"
"-x                do not reset the clock if off by more than one second\n"
"-t                do not adjust the system clock\n"
"-a NUMBER,NUMBER  specify clock servo P and I attenuations\n"
"-w NUMBER         specify one way delay filter stiffness\n"
"\n"
"-b NAME           bind PTP to network interface NAME\n"
"-u ADDRESS        also send uni-cast to ADDRESS\n"
"-e                run in ethernet mode (level2) \n"
"-h                run in End to End mode \n"
"-l NUMBER,NUMBER  specify inbound, outbound latency in nsec\n"

"\n"
"-o NUMBER         specify current UTC offset\n"
"-i NUMBER         specify PTP domain number\n"

"\n"
"-n NUMBER         specify announce interval in 2^NUMBER sec\n"
"-y NUMBER         specify sync interval in 2^NUMBER sec\n"
"-m NUMBER         specify max number of foreign master records\n"
"\n"
"-g                run as slave only\n"
"-v NUMBER         specify system clock allen variance\n"
"-r NUMBER         specify system clock accuracy\n"
"-s NUMBER         specify system clock class\n"
"-p NUMBER         specify priority1 attribute\n"
"-q NUMBER         specify priority2 attribute\n"

"\n"
//"-k NUMBER,NUMBER  send a management message of key, record, then exit\n"  implemented later..
"\n"
      );
      *ret = 0;
      return 0;
      
    case 'c':
      nondaemon = 1;
      break;
    
      
    case 'f':
      if((fd = creat(optarg, 0400)) != -1)
      {
        dup2(fd, STDOUT_FILENO);
        dup2(fd, STDERR_FILENO);
        noclose = 1;
      }
      else
        PERROR("could not open output file");
      break;
      
    case 'd':
#ifndef PTPD_DBG
      rtOpts->displayStats = TRUE;
#endif
      break;
      
    case 'D':
#ifndef PTPD_DBG
      rtOpts->displayStats = TRUE;
      rtOpts->csvStats = TRUE;
#endif
      break;
      
    case 'x':
      rtOpts->noResetClock = TRUE;
      break;
      
    case 't':
      rtOpts->noAdjust = TRUE;
      break;
      
    case 'a':
      rtOpts->ap = strtol(optarg, &optarg, 0);
      if(optarg[0])
        rtOpts->ai = strtol(optarg+1, 0, 0);
      break;
      
    case 'w':
      rtOpts->s = strtol(optarg, &optarg, 0);
      break;
      
    case 'b':
      memset(rtOpts->ifaceName, 0, IFACE_NAME_LENGTH);
      strncpy(rtOpts->ifaceName, optarg, IFACE_NAME_LENGTH);
      break;
      
    case 'u':
      strncpy(rtOpts->unicastAddress, optarg, NET_ADDRESS_LENGTH);
      break;
      
    case 'l':
      rtOpts->inboundLatency.nanoseconds = strtol(optarg, &optarg, 0);
      if(optarg[0])
        rtOpts->outboundLatency.nanoseconds = strtol(optarg+1, 0, 0);
      break;
      
    case 'o':
      rtOpts->currentUtcOffset = strtol(optarg, &optarg, 0);
      break;
      
    case 'i':
      rtOpts->domainNumber = strtol(optarg, &optarg, 0);
      break;
          
    case 'y':
      rtOpts->syncInterval = strtol(optarg, 0, 0);
      break;
      
     case 'n':
     rtOpts->announceInterval=strtol(optarg, 0, 0);
     break;
      
    case 'm':
      rtOpts->max_foreign_records = strtol(optarg, 0, 0);
      if(rtOpts->max_foreign_records < 1)
        rtOpts->max_foreign_records = 1;
      break;
      
    case 'g':
      rtOpts->slaveOnly = TRUE;
      break;
      
    case 'v':
      rtOpts->clockQuality.offsetScaledLogVariance = strtol(optarg, 0, 0);
      break;
      
    case 'r':
      rtOpts->clockQuality.clockAccuracy = strtol(optarg, 0, 0);
      break;
    
    case 's':
      rtOpts->clockQuality.clockClass = strtol(optarg, 0, 0);
      break;
      
    case 'p':
      rtOpts->priority1 = strtol(optarg, 0, 0);
      break;
      
    case 'q':
      rtOpts->priority2 = strtol(optarg, 0, 0);
      break;
      
   case 'e':
      rtOpts->ethernet_mode = TRUE;
      PERROR("Not implemented yet !");
      return 0;
      break;
      
   case 'h':
	   rtOpts->E2E_mode = TRUE;
	   break;
            
    default:
      *ret = 1;
      return 0;
    }
  }
  
  ptpClock = (PtpClock*)calloc(1, sizeof(PtpClock));
  if(!ptpClock)
  {
    PERROR("failed to allocate memory for protocol engine data");
    *ret = 2;
    return 0;
  }
  else
  {
    DBG("allocated %d bytes for protocol engine data\n", (int)sizeof(PtpClock));
    ptpClock->foreign = (ForeignMasterRecord*)calloc(rtOpts->max_foreign_records, sizeof(ForeignMasterRecord));
    if(!ptpClock->foreign)
    {
      PERROR("failed to allocate memory for foreign master data");
      *ret = 2;
      free(ptpClock);
      return 0;
    }
    else
    {
      DBG("allocated %d bytes for foreign master data\n", (int)(rtOpts->max_foreign_records*sizeof(ForeignMasterRecord)));
    }
  }
  
  /*Init to 0 net buffer*/
  memset(ptpClock->msgIbuf,0,PACKET_SIZE);
  memset(ptpClock->msgObuf,0,PACKET_SIZE);
  
  
#ifndef PTPD_NO_DAEMON
  if(!nondaemon)
  {
    if(daemon(0, noclose) == -1)
    {
      PERROR("failed to start as daemon");
      *ret = 3;
      return 0;
    }
    DBG("running as daemon\n");
  }
#endif
  
  signal(SIGINT, catch_close);
  signal(SIGTERM, catch_close);
  signal(SIGHUP, catch_close);
  
  *ret = 0;
     
  return ptpClock;
}
