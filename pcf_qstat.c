
/******************************************************************************/
/* Includes                                                                   */
/******************************************************************************/
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <time.h>

#include <cmqc.h>                          /* MQI                             */
#include <cmqcfc.h>                        /* PCF                             */
#include <cmqbc.h>                         /* MQAI                            */

/******************************************************************************/
/* Function prototypes                                                        */
/******************************************************************************/
void CheckCallResult(MQCHAR *, MQLONG , MQLONG);

/******************************************************************************/
/* Function: main                                                             */
/******************************************************************************/
int main(int argc, char *argv[])
{
   /***************************************************************************/
   /* MQAI variables                                                          */
   /***************************************************************************/
   MQHCONN hConn;                          /* handle to MQ connection         */
   MQCHAR qmName[MQ_Q_MGR_NAME_LENGTH+1]=""; /* default QMgr name             */
   MQCHAR inputq[MQ_Q_NAME_LENGTH+1];    
   MQLONG interval;
   MQLONG reason;                          /* reason code                     */
   MQLONG connReason;                      /* MQCONN reason code              */
   MQLONG compCode;                        /* completion code                 */
   MQHBAG adminBag = MQHB_UNUSABLE_HBAG;   /* admin bag for mqExecute         */
   MQHBAG responseBag = MQHB_UNUSABLE_HBAG;/* response bag for mqExecute      */
   MQHBAG qAttrsBag;                       /* bag containing q attributes     */
   MQHBAG errorBag;                        /* bag containing cmd server error */
   MQLONG mqExecuteCC;                     /* mqExecute completion code       */
   MQLONG mqExecuteRC;                     /* mqExecute reason code           */
   MQLONG qNameLength;                     /* Actual length of q name         */
   MQLONG qhdepth,qenq,qdeq,qtime;                          /* depth of queue                  */
   MQLONG i;                               /* loop counter                    */
   MQLONG numberOfBags;                    /* number of bags in response bag  */
   MQCHAR qName[MQ_Q_NAME_LENGTH+1];       /* name of queue extracted from bag*/
   time_t timer;
   char dtbuffer[26];
   struct tm* tm_info;

   printf("\n Qstats - utility developed by Anand.Victor for MQ Administration purpose\n");
   printf("usage: %s QMNAME QUEUENAME interval\n\n",argv[0]);
   printf("\tTimeStamp\tQueue name\tHigh depth\tDequeue count\tEnqueue Count\tTime interval\n");

   /***************************************************************************/
   /* Connect to the queue manager                                            */
   /***************************************************************************/
   if (argc > 3)
	{
		strncpy(qmName, argv[1], (size_t)MQ_Q_MGR_NAME_LENGTH);
		strncpy(inputq, argv[2], (size_t)MQ_Q_NAME_LENGTH);
		interval=atoi(argv[3]);
	}
	else if (argc == 3)
	{
	    strncpy(qmName, argv[1], (size_t)MQ_Q_MGR_NAME_LENGTH);
		strncpy(inputq, argv[2], (size_t)MQ_Q_NAME_LENGTH);
	}
	else
	{
		printf("usage : qstat qmname queuename interval");
		exit(255);
	}
	//printf("qmname %s", qmName);
do 
{
   MQCONN(qmName, &hConn, &compCode, &connReason);

   /***************************************************************************/
   /* Report the reason and stop if the connection failed.                    */
   /***************************************************************************/
   if (compCode == MQCC_FAILED)
   {
      CheckCallResult("Queue Manager connection", compCode, connReason);
      exit( (int)connReason);
   }

   /***************************************************************************/
   /* Create an admin bag for the mqExecute call                              */
   /***************************************************************************/
   mqCreateBag(MQCBO_ADMIN_BAG, &adminBag, &compCode, &reason);
   CheckCallResult("Create admin bag", compCode, reason);

   /***************************************************************************/
   /* Create a response bag for the mqExecute call                            */
   /***************************************************************************/
   mqCreateBag(MQCBO_ADMIN_BAG, &responseBag, &compCode, &reason);
   CheckCallResult("Create response bag", compCode, reason);

   /***************************************************************************/
   /* Put the generic queue name into the admin bag                           */
   /***************************************************************************/
   mqAddString(adminBag, MQCA_Q_NAME, MQBL_NULL_TERMINATED, inputq, &compCode, &reason);
   CheckCallResult("Add q name", compCode, reason);

   /***************************************************************************/
   /* Put the local queue type into the admin bag                             */
   /***************************************************************************/
 //  mqAddInteger(adminBag, MQIA_Q_TYPE, MQQT_LOCAL, &compCode, &reason);
 //  CheckCallResult("Add q type", compCode, reason);

   /***************************************************************************/
   /* Add an inquiry for current queue depths                                 */
   /***************************************************************************/
  // mqAddInquiry(adminBag, MQIA_CURRENT_Q_DEPTH, &compCode, &reason);
  // CheckCallResult("Add inquiry", compCode, reason);

   /***************************************************************************/
   /* Send the command to find all the local queue names and queue depths.    */
   /* The mqExecute call creates the PCF structure required, sends it to      */
   /* the command server, and receives the reply from the command server into */
   /* the response bag. The attributes are contained in system bags that are  */
   /* embedded in the response bag, one set of attributes per bag.            */
   /***************************************************************************/
   mqExecute(hConn,                   /* MQ connection handle                 */
             MQCMD_RESET_Q_STATS,         /* Command to be executed               */
             MQHB_NONE,               /* No options bag                       */
             adminBag,                /* Handle to bag containing commands    */
             responseBag,             /* Handle to bag to receive the response*/
             MQHO_NONE,               /* Put msg on SYSTEM.ADMIN.COMMAND.QUEUE*/
             MQHO_NONE,               /* Create a dynamic q for the response  */
             &compCode,               /* Completion code from the mqexecute   */
             &reason);                /* Reason code from mqexecute call      */


   /***************************************************************************/
   /* Check the command server is started. If not exit.                       */
   /***************************************************************************/
   if (reason == MQRC_CMD_SERVER_NOT_AVAILABLE)
   {
      printf("Please start the command server: <strmqcsv QMgrName>\n");
      MQDISC(&hConn, &compCode, &reason);
      CheckCallResult("Disconnect from Queue Manager", compCode, reason);
      exit(98);
   }

   /***************************************************************************/
   /* Check the result from mqExecute call. If successful find the current    */
   /* depths of all the local queues. If failed find the error.               */
   /***************************************************************************/
   if ( compCode == MQCC_OK )                      /* Successful mqExecute    */
   {
     /*************************************************************************/
     /* Count the number of system bags embedded in the response bag from the */
     /* mqExecute call. The attributes for each queue are in a separate bag.  */
     /*************************************************************************/
     mqCountItems(responseBag, MQHA_BAG_HANDLE, &numberOfBags, &compCode, &reason);
     CheckCallResult("Count number of bag handles", compCode, reason);

     for ( i=0; i<numberOfBags; i++)
     {
       /***********************************************************************/
       /* Get the next system bag handle out of the mqExecute response bag.   */
       /* This bag contains the queue attributes                              */
       /***********************************************************************/
       mqInquireBag(responseBag, MQHA_BAG_HANDLE, i, &qAttrsBag, &compCode, &reason);
       CheckCallResult("Get the result bag handle", compCode, reason);

       /***********************************************************************/
       /* Get the queue name out of the queue attributes bag                  */
       /***********************************************************************/
       mqInquireString(qAttrsBag, MQCA_Q_NAME, 0, MQ_Q_NAME_LENGTH, qName,
                       &qNameLength, NULL, &compCode, &reason);
       CheckCallResult("Get queue name", compCode, reason);

       /***********************************************************************/
       /* Get the depth out of the queue attributes bag                       */
       /***********************************************************************/
	   mqInquireInteger(qAttrsBag, MQIA_HIGH_Q_DEPTH, MQIND_NONE, &qhdepth,&compCode, &reason);
	   mqInquireInteger(qAttrsBag, MQIA_MSG_DEQ_COUNT, MQIND_NONE, &qdeq,&compCode, &reason);
	   mqInquireInteger(qAttrsBag, MQIA_MSG_ENQ_COUNT, MQIND_NONE, &qenq,&compCode, &reason);
	   mqInquireInteger(qAttrsBag, MQIA_TIME_SINCE_RESET, MQIND_NONE, &qtime,&compCode, &reason);
      // mqInquireInteger(qAttrsBag, MQIA_CURRENT_Q_DEPTH, MQIND_NONE, &qDepth,                        &compCode, &reason);
      // CheckCallResult("Get depth", compCode, reason);

       /***********************************************************************/
       /* Use mqTrim to prepare the queue name for printing.                  */
       /* Print the result.                                                   */
       /***********************************************************************/
       mqTrim(MQ_Q_NAME_LENGTH, qName, qName, &compCode, &reason);
       time(&timer);
       tm_info = localtime(&timer);
       strftime(dtbuffer, 26, "%Y:%m:%d %H:%M:%S", tm_info);
       printf("%s\t%s\t%ld\t\t%ld\t\t%ld\t\t%ld \n", dtbuffer, qName, qhdepth, qdeq ,qenq, qtime);
     }
   }

   else                                               /* Failed mqExecute     */
   {
     printf("Call to get queue attributes failed: Completion Code = %d : Reason = %d\n",
                  compCode, reason);
     /*************************************************************************/
     /* If the command fails get the system bag handle out of the mqexecute   */
     /* response bag.This bag contains the reason from the command server     */
     /* why the command failed.                                               */
     /*************************************************************************/
     if (reason == MQRCCF_COMMAND_FAILED)
     {
       mqInquireBag(responseBag, MQHA_BAG_HANDLE, 0, &errorBag, &compCode, &reason);
       CheckCallResult("Get the result bag handle", compCode, reason);

       /***********************************************************************/
       /* Get the completion code and reason code, returned by the command    */
       /* server, from the embedded error bag.                                */
       /***********************************************************************/
       mqInquireInteger(errorBag, MQIASY_COMP_CODE, MQIND_NONE, &mqExecuteCC,
                        &compCode, &reason );
       CheckCallResult("Get the completion code from the result bag", compCode, reason);
       mqInquireInteger(errorBag, MQIASY_REASON, MQIND_NONE, &mqExecuteRC,
                         &compCode, &reason);
       CheckCallResult("Get the reason code from the result bag", compCode, reason);
       printf("Error returned by the command server: Completion Code = %d : Reason = %d\n",
                mqExecuteCC, mqExecuteRC);
     }
   }

   /***************************************************************************/
   /* Delete the admin bag if successfully created.                           */
   /***************************************************************************/
   if (adminBag != MQHB_UNUSABLE_HBAG)
   {
      mqDeleteBag(&adminBag, &compCode, &reason);
      CheckCallResult("Delete the admin bag", compCode, reason);
   }

   /***************************************************************************/
   /* Delete the response bag if successfully created.                        */
   /***************************************************************************/
   if (responseBag != MQHB_UNUSABLE_HBAG)
   {
      mqDeleteBag(&responseBag, &compCode, &reason);
      CheckCallResult("Delete the response bag", compCode, reason);
   }

   /***************************************************************************/
   /* Disconnect from the queue manager if not already connected              */
   /***************************************************************************/
   if (connReason != MQRC_ALREADY_CONNECTED)
   {
      MQDISC(&hConn, &compCode, &reason);
       CheckCallResult("Disconnect from Queue Manager", compCode, reason);
   }
   sleep(interval);
}while(1); 
  return 0;
   
}


/******************************************************************************/
/*                                                                            */
/* Function: CheckCallResult                                                  */
/*                                                                            */
/******************************************************************************/
/*                                                                            */
/* Input Parameters:  Description of call                                     */
/*                    Completion code                                         */
/*                    Reason code                                             */
/*                                                                            */
/* Output Parameters: None                                                    */
/*                                                                            */
/* Logic: Display the description of the call, the completion code and the    */
/*        reason code if the completion code is not successful                */
/*                                                                            */
/******************************************************************************/
void  CheckCallResult(char *callText, MQLONG cc, MQLONG rc)
{
   if (cc != MQCC_OK)
         printf("%s failed: Completion Code = %d : Reason = %d\n", callText, cc, rc);

}

