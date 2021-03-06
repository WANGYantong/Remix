#include "remix_private.h"

U32 guiSystemStatus;

REMIX_TASKSCHEDTAB gstrReadyTab;
REMIX_DLIST gstrDelayTab;

U32 guiTick;
U8 gucTickSched;

STACKREG *gpstrCurTaskReg;
STACKREG *gpstrNextTaskReg;

REMIX_TCB *gpstrCurTcb;
REMIX_TCB *gpstrRootTaskTcb;
REMIX_TCB *gpstrIdleTaskTcb;

U32 guiUser;

#ifdef REMIX_UNMAP
const U8 caucTaskPrioUnmapTab[256] = {
	0, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0,
	4, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0,
	5, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0,
	4, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0,
	6, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0,
	4, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0,
	5, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0,
	4, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0,
	7, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0,
	4, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0,
	5, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0,
	4, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0,
	6, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0,
	4, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0,
	5, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0,
	4, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0
};
#endif

void REMIXOS_Init(void)
{
	REMIX_SetUser(USERROOT);

	guiSystemStatus = SYSTEMNOTSCHEDULE;

	gpstrCurTcb = (REMIX_TCB *) NULL;
	gpstrIdleTaskTcb = (REMIX_TCB *) NULL;
	gpstrRootTaskTcb = (REMIX_TCB *) NULL;

	guiTick = 0;
	gucTickSched = TICKSCHEDCLR;

	guiIntLockCounter = 0;

#ifdef REMIX_INCLUDETASKHOOK
	REMIX_TaskHookInit();
#endif

#ifdef REMIX_TASKROUNDROBIN
	REMIX_TaskTimeSlice(0, TASKTIMESLICEALLPRIO);
#endif

#ifdef REMIX_CPUSTATISTIC
	guiCpuSharePeriod = 0;
#endif

#ifdef REMIX_MEMSTATIC
	REMIX_MemInit();
#endif

	REMIX_TaskListInit();
	REMIX_TaskReadyTableInit();
	REMIX_TaskDelayTableInit();

	gpstrRootTaskTcb =
	    REMIX_TaskCreate(ROOTTASKNAME, REMIX_BeforeRootTask, NULL, NULL, ROOTTASKSTACK, USERHIGHESTPRIORITY, NULL);
	gpstrIdleTaskTcb =
	    REMIX_TaskCreate(ROOTIDLENAME, REMIX_IdleTask, NULL, NULL, IDLETASKSTACK, LOWESTPRIORITY, NULL);

	REMIX_SystemHardwareInit();

}

void REMIXOS_Start(void)
{
	gpstrNextTaskReg = &gpstrRootTaskTcb->strStackReg;
	gpstrCurTcb = gpstrRootTaskTcb;

	REMIX_SwitchToTask();
}


void REMIX_BeforeRootTask(void *pvPara)
{
	guiSystemStatus = SYSTEMSCHEDULE;

	REMIX_SetUser(USERGUEST);
}


void REMIX_TaskTick(void)
{
	guiTick++;
	gucTickSched = TICKSCHEDSET;

#ifdef REMIX_CPUSTATISTIC
	guiCpuSharePeriod++;
#endif

#ifdef REMIX_TASKROUNDROBIN
	if (NULL != gpstrCurTcb) {
		if (0 != guiTimeSlice[gpstrCurTcb->ucTaskPrio]) {
			gauiSliceCnt[gpstrCurTcb->ucTaskPrio]++;
		}
	}
#endif

	REMIX_IntPendSvSet();
}

void REMIX_TaskSched(void)
{
	REMIX_TCB *pstrTcb;

	if (TICKSCHEDSET == gucTickSched) {
		gucTickSched = TICKSCHEDCLR;
		REMIX_TaskDelayTableSched();
	}

	pstrTcb = REMIX_TaskReadyTableSched();

#ifdef REMIX_CPUSTATISTIC
	REMIX_CPUShareStatistic(gpstrCurTcb, pstrTcb);
#endif

#ifdef REMIX_INCLUDETASKHOOK
	if ((VFHSWT) NULL != gvfTaskSwitchHook) {
		gvfTaskSwitchHook(gpstrCurTcb, pstrTcb);
	}
#endif

	if (NULL != gpstrCurTcb) {
		gpstrCurTaskReg = &gpstrCurTcb->strStackReg;
	} else {
		gpstrCurTaskReg = NULL;
	}

	gpstrNextTaskReg = &pstrTcb->strStackReg;
	gpstrCurTcb = pstrTcb;

}

/**************************************************************************/
void REMIX_TaskListInit(void)
{
	REMIX_DlistInit(&gstrTaskList);
}

void REMIX_TaskAddToTaskList(REMIX_DLIST * pstrNode)
{
	REMIX_DlistNodeAdd(&gstrTaskList, pstrNode);
}

REMIX_DLIST *REMIX_TaskDeleteFromTaskList(REMIX_DLIST * pstrNode)
{
	return REMIX_DlistCurNodeDelete(&gstrTaskList, pstrNode);
}

/**************************************************************************/

/**************************************************************************/
void REMIX_TaskReadyTableInit(void)
{
	REMIX_TaskSchedTableInit(&gstrReadyTab);
}

void REMIX_TaskAddToReadyTable(REMIX_DLIST * pstrList, REMIX_DLIST * pstrNode, REMIX_PRIOFLAG * pstrPrioFlag,
			       PRIORITYBITS ucTaskPrio)
{
	REMIX_TaskAddToSchedTable(pstrList, pstrNode, pstrPrioFlag, ucTaskPrio);
}

REMIX_DLIST *REMIX_TaskDeleteFromReadyTable(REMIX_DLIST * pstrList, REMIX_DLIST * pstrNode, REMIX_PRIOFLAG * pstrPrioFlag,
					    PRIORITYBITS ucTaskPrio)
{
	return REMIX_TaskDeleteFromSchedTable(pstrList, pstrNode, pstrPrioFlag, ucTaskPrio);
}

REMIX_TCB *REMIX_TaskReadyTableSched(void)
{
	REMIX_TCB *pstrTcb;
	REMIX_DLIST *pstrList;
	REMIX_DLIST *pstrNode;
#ifdef REMIX_TASKROUNDROBIN
	REMIX_DLIST *pstrNextNode;
#endif
	REMIX_TCBQUE *pstrTaskQue;
	PRIORITYBITS ucTaskPrio;

	ucTaskPrio = REMIX_TaskGetHighestPrio(&gstrReadyTab.strFlag);
	pstrList = &gstrReadyTab.astrList[ucTaskPrio];
	pstrNode = REMIX_DlistEmpInq(pstrList);
	pstrTaskQue = (REMIX_TCBQUE *) pstrNode;
	pstrTcb = pstrTaskQue->pstrTcb;

#ifdef REMIX_TASKROUNDROBIN

	if (0 != guiTimeSlice[gpstrCurTcb->ucTaskPrio]) {
		if (gpstrCurTcb == pstrTcb) {
			if (gauiSliceCnt[gpstrCurTcb->ucTaskPrio] >= guiTimeSlice[gpstrCurTcb->ucTaskPrio]) {
				gauiSliceCnt[gpstrCurTcb->ucTaskPrio] = 0;
				pstrNextNode = REMIX_DlistNextNodeEmpInq(pstrList, pstrNode);

				if (NULL != pstrNextNode) {
					(void) REMIX_DlistNodeDelete(pstrList);
					REMIX_DlistNodeAdd(pstrList, pstrNode);

					pstrTaskQue = (REMIX_TCBQUE *) pstrNextNode;
					pstrTcb = pstrTaskQue->pstrTcb;
				}
			}
		}
	}
#endif

	return pstrTcb;

}

/**************************************************************************/

/**************************************************************************/
void REMIX_TaskDelayTableInit(void)
{
	REMIX_DlistInit(&gstrDelayTab);
}

void REMIX_TaskAddToDelayTable(REMIX_DLIST * pstrNode)
{
	REMIX_DLIST *pstrTempNode;
	REMIX_DLIST *pstrNextNode;
	REMIX_TCBQUE *pstrTcbQue;
	U32 uiStillTick;
	U32 uiTempStillTick;

	pstrTempNode = REMIX_DlistEmpInq(&gstrDelayTab);

	if (NULL != pstrTempNode) {
		pstrTcbQue = (REMIX_TCBQUE *) pstrNode;
		uiStillTick = pstrTcbQue->pstrTcb->uiStillTick;

		while (1) {
			pstrTcbQue = (REMIX_TCBQUE *) pstrTempNode;
			uiTempStillTick = pstrTcbQue->pstrTcb->uiStillTick;

			if (((guiTick < uiStillTick) && (uiStillTick < uiTempStillTick))
			    || ((uiStillTick < uiTempStillTick) && (uiTempStillTick < guiTick))
			    || ((uiTempStillTick < guiTick) && (guiTick < uiStillTick))) {

				REMIX_DlistCurNodeInsert(&gstrDelayTab, pstrTempNode, pstrNode);
				return;

			}

			pstrNextNode = REMIX_DlistNextNodeEmpInq(&gstrDelayTab, pstrTempNode);

			if (NULL != pstrNextNode) {
				pstrTempNode = pstrNextNode;
			} else {
				REMIX_DlistNodeAdd(&gstrDelayTab, pstrNode);
				return;
			}
		}
	} else {
		REMIX_DlistNodeAdd(&gstrDelayTab, pstrNode);
		return;
	}
}

REMIX_DLIST *REMIX_TaskDeleteFromDelayTable(REMIX_DLIST * pstrNode)
{
	return REMIX_DlistCurNodeDelete(&gstrDelayTab, pstrNode);
}

void REMIX_TaskDelayTableSched(void)
{
	REMIX_TCB *pstrTcb;
	REMIX_DLIST *pstrList;
	REMIX_DLIST *pstrNode;
	REMIX_DLIST *pstrDelayNode;
	REMIX_DLIST *pstrNextNode;
	REMIX_PRIOFLAG *pstrPrioFlag;
	REMIX_TCBQUE *pstrTcbQue;
	U32 uiTick;
	PRIORITYBITS ucTaskPrio;

	pstrDelayNode = REMIX_DlistEmpInq(&gstrDelayTab);

	if (NULL != pstrDelayNode) {
		while (1) {
			pstrTcbQue = (REMIX_TCBQUE *) pstrDelayNode;
			pstrTcb = pstrTcbQue->pstrTcb;
			uiTick = pstrTcb->uiStillTick;

			if (uiTick == guiTick) {
				pstrNextNode = REMIX_TaskDeleteFromDelayTable(pstrDelayNode);
				pstrTcb->uiTaskFlag &= (~((U32) DELAYQUEFLAG));

				if (TASKDELAY == (TASKDELAY & pstrTcb->strTaskOpt.ucTaskSta)) {
					pstrTcb->strTaskOpt.ucTaskSta &= ~((U8) TASKDELAY);
					pstrTcb->strTaskOpt.uiDelayTick = RTN_TASKDELAYTIMEOUT;
				} else if (TASKPEND == (TASKPEND & pstrTcb->strTaskOpt.ucTaskSta)) {

#ifdef REMIX_SEMGROUPFLAG
					if (TASKSEMGROUPFLAG == (pstrTcb->uiTaskFlag & TASKSEMGROUPFLAG)) {
						(void) REMIX_TaskDeleteFromFlagTable(pstrTcb);
						pstrTcb->uiTaskFlag &= (~((U32) TASKSEMGROUPFLAG));
						pstrTcb->strTaskOpt.uiDelayTick = RTN_FLAGTASKTIMEOUT;
					} else
#endif
					{
						(void) REMIX_TaskDeleteFromSemTable(pstrTcb);
						pstrTcb->strTaskOpt.uiDelayTick = RTN_SEMTASKTIMEOUT;
					}
					pstrTcb->strTaskOpt.ucTaskSta &= ~((U8) TASKPEND);
				}

				pstrNode = &pstrTcb->strTcbQue.strQueHead;
				ucTaskPrio = pstrTcb->ucTaskPrio;
				pstrList = &gstrReadyTab.astrList[ucTaskPrio];
				pstrPrioFlag = &gstrReadyTab.strFlag;

				REMIX_TaskAddToReadyTable(pstrList, pstrNode, pstrPrioFlag, ucTaskPrio);

				pstrTcb->strTaskOpt.ucTaskSta |= TASKREADY;

				if (NULL == pstrNextNode) {
					break;
				} else {
					pstrDelayNode = pstrNextNode;
				}
			} else {
				break;
			}
		}
	}
}

/**************************************************************************/

/**************************************************************************/
void REMIX_TaskSemTableInit(REMIX_TASKSCHEDTAB * pstrSchedTab)
{
	REMIX_TaskSchedTableInit(pstrSchedTab);
}

void REMIX_TaskAddToSemTable(REMIX_TCB * pstrTcb, REMIX_SEM * pstrSem)
{
	REMIX_DLIST *pstrList;
	REMIX_DLIST *pstrNode;
	REMIX_PRIOFLAG *pstrPrioFlag;
	PRIORITYBITS ucTaskPrio;

	if (SEMPRIO == (SEMSCHEDULEMASK & pstrSem->uiSemOpt)) {
		ucTaskPrio = pstrTcb->ucTaskPrio;
		pstrNode = &pstrTcb->strSemQue.strQueHead;

		pstrList = &pstrSem->strSemTab.astrList[ucTaskPrio];
		pstrPrioFlag = &pstrSem->strSemTab.strFlag;

		REMIX_TaskAddToSchedTable(pstrList, pstrNode, pstrPrioFlag, ucTaskPrio);
	} else {
		pstrList = &pstrSem->strSemTab.astrList[LOWESTPRIORITY];
		pstrNode = &pstrTcb->strSemQue.strQueHead;

		REMIX_DlistNodeAdd(pstrList, pstrNode);
	}

	pstrTcb->pstrSem = pstrSem;
}

REMIX_DLIST *REMIX_TaskDeleteFromSemTable(REMIX_TCB * pstrTcb)
{
	REMIX_SEM *pstrSem;
	REMIX_DLIST *pstrList;
    REMIX_DLIST* pstrNode;
	REMIX_PRIOFLAG *pstrPrioFlag;
	PRIORITYBITS ucTaskPrio;

	pstrSem = pstrTcb->pstrSem;
    pstrNode=&pstrTcb->strSemQue.strQueHead;

	if (SEMPRIO == (SEMSCHEDULEMASK & pstrSem->uiSemOpt)) {
		ucTaskPrio = pstrTcb->ucTaskPrio;
		pstrList = &pstrSem->strSemTab.astrList[ucTaskPrio];
		pstrPrioFlag = &pstrSem->strSemTab.strFlag;

		return REMIX_TaskDeleteFromSchedTable(pstrList, pstrNode, pstrPrioFlag, ucTaskPrio);
	} else {
		pstrList = &pstrSem->strSemTab.astrList[LOWESTPRIORITY];

		return REMIX_DlistCurNodeDelete(pstrList, pstrNode);
	}
}

REMIX_TCB *REMIX_TaskSemTableSche(REMIX_SEM * pstrSem)
{
	REMIX_DLIST *pstrNode;
	REMIX_TCBQUE *pstrTaskQue;
	PRIORITYBITS ucTaskPrio;

	if (SEMPRIO == (SEMSCHEDULEMASK & pstrSem->uiSemOpt)) {
		ucTaskPrio = REMIX_TaskGetHighestPrio(&pstrSem->strSemTab.strFlag);
	} else {
		ucTaskPrio = LOWESTPRIORITY;
	}

	pstrNode = REMIX_DlistEmpInq(&pstrSem->strSemTab.astrList[ucTaskPrio]);

	if (NULL == pstrNode) {
		return (REMIX_TCB *) NULL;
	} else {
		pstrTaskQue = (REMIX_TCBQUE *) pstrNode;
		return pstrTaskQue->pstrTcb;
	}
}

/**************************************************************************/

/**************************************************************************/
#ifdef REMIX_SEMGROUPFLAG

void REMIX_TaskFlagTableInit(REMIX_TASKSCHEDTAB * pstrSchedTab)
{
	REMIX_TaskSchedTableInit(pstrSchedTab);
}

void REMIX_TaskAddToFlagTable(REMIX_TCB * pstrTcb, REMIX_FLAG * pstrFlag)
{
	REMIX_DLIST *pstrList;
	REMIX_DLIST *pstrNode;
	REMIX_PRIOFLAG *pstrPrioFlag;
	PRIORITYBITS ucTaskPrio;

	if (REMIXFLAGSCHEDPRIO == (REMIXFLAGSCHEDMASK & pstrFlag->uiFlagOpt)) {
		ucTaskPrio = pstrTcb->ucTaskPrio;
		pstrNode = &pstrTcb->strSemQue.strQueHead;

		pstrList = &pstrFlag->strFlagTab.astrList[ucTaskPrio];
		pstrPrioFlag = &pstrFlag->strFlagTab.strFlag;

		REMIX_TaskAddToSchedTable(pstrList, pstrNode, pstrPrioFlag, ucTaskPrio);
	} else {
		pstrList = &pstrFlag->strFlagTab.astrList[LOWESTPRIORITY];
		pstrNode = &pstrTcb->strSemQue.strQueHead;

		REMIX_DlistNodeAdd(pstrList, pstrNode);
	}

}

REMIX_DLIST *REMIX_TaskDeleteFromFlagTable(REMIX_TCB * pstrTcb)
{
	REMIX_FLAG *pstrFlag;
	REMIX_DLIST *pstrList;
    REMIX_DLIST *pstrNode;
	REMIX_PRIOFLAG *pstrPrioFlag;
	PRIORITYBITS ucTaskPrio;

	pstrFlag = pstrTcb->strTaskNodeFlag.pRemixFlag;

	if (REMIXFLAGSCHEDPRIO == (REMIXFLAGSCHEDMASK & pstrFlag->uiFlagOpt)) {
        pstrNode=&pstrTcb->strSemQue.strQueHead;
        ucTaskPrio = pstrTcb->ucTaskPrio;
		pstrList = &pstrFlag->strFlagTab.astrList[ucTaskPrio];
		pstrPrioFlag = &pstrFlag->strFlagTab.strFlag;

		return REMIX_TaskDeleteFromSchedTable(pstrList, pstrNode, pstrPrioFlag, ucTaskPrio);
	} else {
		pstrList = &pstrFlag->strFlagTab.astrList[LOWESTPRIORITY];

		return REMIX_DlistNodeDelete(pstrList);
	}
}

REMIX_TCB *REMIX_TaskFlagTableSche(REMIX_FLAG * pstrFlag)
{
	REMIX_DLIST *pstrNode;
	REMIX_TCBQUE *pstrTaskQue;
	REMIX_TCB *pstrTcb;
	PRIORITYBITS ucTaskPrio;
	U32 uiFlagSta;

	if (REMIXFLAGSCHEDFIFO == (pstrFlag->uiFlagOpt & REMIXFLAGSCHEDMASK)) {
		ucTaskPrio = LOWESTPRIORITY;
		pstrNode = REMIX_DlistEmpInq(&pstrFlag->strFlagTab.astrList[ucTaskPrio]);
		while (1) {

			if (NULL == pstrNode) {
				return (REMIX_TCB *) NULL;
			}

			pstrTaskQue = (REMIX_TCBQUE *) pstrNode;
			pstrTcb = pstrTaskQue->pstrTcb;

			switch (pstrTcb->strTaskNodeFlag.uiFlagNodeOpt) {

			case REMIXFLAGWAITSETAND:
				uiFlagSta = pstrFlag->uiFlagNowBit & pstrTcb->strTaskNodeFlag.uiFlagWantBit;
				if (uiFlagSta == pstrTcb->strTaskNodeFlag.uiFlagWantBit) {
					return pstrTcb;
				} else {
					break;
				}

			case REMIXFLAGWAITSETOR:
				uiFlagSta = pstrFlag->uiFlagNowBit & pstrTcb->strTaskNodeFlag.uiFlagWantBit;
				if (uiFlagSta != (U32) 0) {
					return pstrTcb;
				} else {
					break;
				}

			case REMIXFLAGWAITCLRAND:
				uiFlagSta = ~pstrFlag->uiFlagNowBit & pstrTcb->strTaskNodeFlag.uiFlagWantBit;
				if (uiFlagSta == pstrTcb->strTaskNodeFlag.uiFlagWantBit) {
					return pstrTcb;
				} else {
					break;
				}

			case REMIXFLAGWAITCLROR:
				uiFlagSta = ~pstrFlag->uiFlagNowBit & pstrTcb->strTaskNodeFlag.uiFlagWantBit;
				if (uiFlagSta != (U32) 0) {
					return pstrTcb;
				} else {
					break;
				}

			default:
				break;
			}

			pstrNode = REMIX_DlistNextNodeEmpInq(&pstrFlag->strFlagTab.astrList[ucTaskPrio], pstrNode);
		}
	} else {
		ucTaskPrio = REMIX_TaskGetHighestPrio(&pstrFlag->strFlagTab.strFlag);
		pstrNode = REMIX_DlistEmpInq(&pstrFlag->strFlagTab.astrList[ucTaskPrio]);
		while (1) {
			while (NULL == pstrNode) {
				ucTaskPrio++;
				if ((LOWESTPRIORITY + 1) == ucTaskPrio) {
					return (REMIX_TCB *) NULL;
				} else {
					pstrNode = REMIX_DlistEmpInq(&pstrFlag->strFlagTab.astrList[ucTaskPrio]);
				}
			}

			pstrTaskQue = (REMIX_TCBQUE *) pstrNode;
			pstrTcb = pstrTaskQue->pstrTcb;

			switch (pstrTcb->strTaskNodeFlag.uiFlagNodeOpt) {

			case REMIXFLAGWAITSETAND:
				uiFlagSta = pstrFlag->uiFlagNowBit & pstrTcb->strTaskNodeFlag.uiFlagWantBit;
				if (uiFlagSta == pstrTcb->strTaskNodeFlag.uiFlagWantBit) {
					return pstrTcb;
				} else {
					break;
				}

			case REMIXFLAGWAITSETOR:
				uiFlagSta = pstrFlag->uiFlagNowBit & pstrTcb->strTaskNodeFlag.uiFlagWantBit;
				if (uiFlagSta != (U32) 0) {
					return pstrTcb;
				} else {
					break;
				}

			case REMIXFLAGWAITCLRAND:
				uiFlagSta = ~pstrFlag->uiFlagNowBit & pstrTcb->strTaskNodeFlag.uiFlagWantBit;
				if (uiFlagSta == pstrTcb->strTaskNodeFlag.uiFlagWantBit) {
					return pstrTcb;
				} else {
					break;
				}

			case REMIXFLAGWAITCLROR:
				uiFlagSta = ~pstrFlag->uiFlagNowBit & pstrTcb->strTaskNodeFlag.uiFlagWantBit;
				if (uiFlagSta != (U32) 0) {
					return pstrTcb;
				} else {
					break;
				}

			default:
				break;
			}

			pstrNode = REMIX_DlistNextNodeEmpInq(&pstrFlag->strFlagTab.astrList[ucTaskPrio], pstrNode);
		}
	}
}

REMIX_TCB *REMIX_TaskFlagTableCheck(REMIX_FLAG * pstrFlag)
{
	REMIX_DLIST *pstrNode;
	REMIX_TCBQUE *pstrTaskQue;
	PRIORITYBITS ucTaskPrio;

	if (REMIXFLAGSCHEDFIFO == (pstrFlag->uiFlagOpt & REMIXFLAGSCHEDMASK)) {
		ucTaskPrio = REMIX_TaskGetHighestPrio(&pstrFlag->strFlagTab.strFlag);
	} else {
		ucTaskPrio = LOWESTPRIORITY;
	}

	pstrNode = REMIX_DlistEmpInq(&pstrFlag->strFlagTab.astrList[ucTaskPrio]);

	if (NULL == pstrNode) {
		return (REMIX_TCB *) NULL;
	} else {
		pstrTaskQue = (REMIX_TCBQUE *) pstrNode;
		return pstrTaskQue->pstrTcb;
	}

}

#endif

/**************************************************************************/

/*************************************************************************/
void REMIX_TaskSchedTableInit(REMIX_TASKSCHEDTAB * pstrSchedTab)
{
	U32 i;

	for (i = 0; i < PRIORITYNUM; i++) {
		REMIX_DlistInit(&pstrSchedTab->astrList[i]);
	}

#if PRIORITYNUM >= PRIORITY1024

	for (i = 0; i < PRIOFLAGGRP1; i++) {
		pstrSchedTab->strFlag.aucPrioFlagGrp1[i] = 0;
	}

	for (i = 0; i < PRIOFLAGGRP2; i++) {
		pstrSchedTab->strFlag.aucPrioFlagGrp2[i] = 0;
	}

	for (i = 0; i < PRIOFLAGGPR3; i++) {
		pstrSchedTab->strFlag.aucPrioFlagGrp3[i] = 0;
	}

	pstrSchedTab->strFlag.ucPrioFlagGrp4 = 0;

#elif PRIORITYNUM >= PRIORITY128

	for (i = 0; i < PRIOFLAGGRP1; i++) {
		pstrSchedTab->strFlag.aucPrioFlagGrp1[i] = 0;
	}

	for (i = 0; i < PRIOFLAGGRP2; i++) {
		pstrSchedTab->strFlag.aucPrioFlagGrp2[i] = 0;
	}

	pstrSchedTab->strFlag.ucPrioFlagGrp3 = 0;

#elif PRIORITYNUM >= PRIORITY16

	for (i = 0; i < PRIOFLAGGRP1; i++) {
		pstrSchedTab->strFlag.aucPrioFlagGrp1[i] = 0;
	}

	pstrSchedTab->strFlag.ucPrioFlagGrp2 = 0;

#else

	pstrSchedTab->strFlag.ucPrioFlagGrp1 = 0;

#endif
}

void REMIX_TaskAddToSchedTable(REMIX_DLIST * pstrList, REMIX_DLIST * pstrNode,
			       REMIX_PRIOFLAG * pstrPrioFlag, PRIORITYBITS ucTaskPrio)
{
	REMIX_DlistNodeAdd(pstrList, pstrNode);
	REMIX_TaskSetPrioFlag(pstrPrioFlag, ucTaskPrio);
}

REMIX_DLIST *REMIX_TaskDeleteFromSchedTable(REMIX_DLIST * pstrList, REMIX_DLIST * pstrNode, REMIX_PRIOFLAG * pstrPrioFlag,
					    PRIORITYBITS ucTaskPrio)
{
	REMIX_DLIST *pstrDelNode;

    if((REMIX_DLIST*)NULL!=pstrNode){
        pstrDelNode = REMIX_DlistCurNodeDelete(pstrList, pstrNode);
    }
	else{
        pstrDelNode=REMIX_DlistNodeDelete(pstrList);
    }

	if (NULL == REMIX_DlistEmpInq(pstrList)) {
		REMIX_TaskClrPrioFlag(pstrPrioFlag, ucTaskPrio);
	}

	return pstrDelNode;
}

/*************************************************************************/

void REMIX_TaskSetPrioFlag(REMIX_PRIOFLAG * pstrPrioFlag, PRIORITYBITS ucTaskPrio)
{
#if PRIORITYNUM >= PRIORITY1024
	U8 ucPrioFlagGrp1;
	U8 ucPrioFlagGrp2;
	U8 ucPrioFlagGrp3;
	U8 ucPositionInGrp1;
	U8 ucPositionInGrp2;
	U8 ucPositionInGrp3;
	U8 ucPositionInGrp4;
#elif PRIORITYNUM >= PRIORITY128
	U8 ucPrioFlagGrp1;
	U8 ucPrioFlagGrp2;
	U8 ucPositionInGrp1;
	U8 ucPositionInGrp2;
	U8 ucPositionInGrp3;
#elif PRIORITYNUM >= PRIORITY16
	U8 ucPrioFlagGrp1;
	U8 ucPositionInGrp1;
	U8 ucPositionInGrp2;
#endif

#if PRIORITYNUM >= PRIORITY1024
	ucPrioFlagGrp1 = ucTaskPrio / 8;
	ucPrioFlagGrp2 = ucPrioFlagGrp1 / 8;
	ucPrioFlagGrp3 = ucPrioFlagGrp2 / 8;

	ucPositionInGrp1 = ucTaskPrio % 8;
	ucPositionInGrp2 = ucPrioFlagGrp1 % 8;
	ucPositionInGrp3 = ucPrioFlagGrp2 % 8;
	ucPositionInGrp4 = ucPrioFlagGrp3;

	pstrPrioFlag->aucPrioFlagGrp1[ucPrioFlagGrp1] |= (U8) (1 << ucPositionInGrp1);
	pstrPrioFlag->aucPrioFlagGrp2[ucPrioFlagGrp2] |= (U8) (1 << ucPositionInGrp2);
	pstrPrioFlag->aucPrioFlagGrp3[ucPrioFlagGrp3] |= (U8) (1 << ucPositionInGrp3);
	pstrPrioFlag->ucPrioFlagGrp4 |= (U8) (1 << ucPositionInGrp4);
#elif PRIORITYNUM >= PRIORITY128
	ucPrioFlagGrp1 = ucTaskPrio / 8;
	ucPrioFlagGrp2 = ucPrioFlagGrp1 / 8;

	ucPositionInGrp1 = ucTaskPrio % 8;
	ucPositionInGrp2 = ucPrioFlagGrp1 % 8;
	ucPositionInGrp3 = ucPrioFlagGrp2;

	pstrPrioFlag->aucPrioFlagGrp1[ucPrioFlagGrp1] |= (U8) (1 << ucPositionInGrp1);
	pstrPrioFlag->aucPrioFlagGrp2[ucPrioFlagGrp2] |= (U8) (1 << ucPositionInGrp2);
	pstrPrioFlag->ucPrioFlagGrp3 |= (U8) (1 << ucPositionInGrp3);
#elif PRIORITYNUM >= PRIORITY16
	ucPrioFlagGrp1 = ucTaskPrio / 8;

	ucPositionInGrp1 = ucTaskPrio % 8;
	ucPositionInGrp2 = ucPrioFlagGrp1;

	pstrPrioFlag->aucPrioFlagGrp1[ucPrioFlagGrp1] |= (U8) (1 << ucPositionInGrp1);
	pstrPrioFlag->ucPrioFlagGrp2 |= (U8) (1 << ucPositionInGrp2);
#else
	pstrPrioFlag->ucPrioFlagGrp1 |= (U8) (1 << ucTaskPrio);
#endif

}

void REMIX_TaskClrPrioFlag(REMIX_PRIOFLAG * pstrPrioFlag, PRIORITYBITS ucTaskPrio)
{
#if PRIORITYNUM >= PRIORITY1024
	U8 ucPrioFlagGrp1;
	U8 ucPrioFlagGrp2;
	U8 ucPrioFlagGrp3;
	U8 ucPositionInGrp1;
	U8 ucPositionInGrp2;
	U8 ucPositionInGrp3;
	U8 ucPositionInGrp4;
#elif PRIORITYNUM >= PRIORITY128
	U8 ucPrioFlagGrp1;
	U8 ucPrioFlagGrp2;
	U8 ucPositionInGrp1;
	U8 ucPositionInGrp2;
	U8 ucPositionInGrp3;
#elif PRIORITYNUM >= PRIORITY16
	U8 ucPrioFlagGrp1;
	U8 ucPositionInGrp1;
	U8 ucPositionInGrp2;
#endif

#if PRIORITYNUM >= PRIORITY1024
	ucPrioFlagGrp1 = ucTaskPrio / 8;
	ucPrioFlagGrp2 = ucPrioFlagGrp1 / 8;
	ucPrioFlagGrp3 = ucPrioFlagGrp2 / 8;

	ucPositionInGrp1 = ucTaskPrio % 8;
	ucPositionInGrp2 = ucPrioFlagGrp1 % 8;
	ucPositionInGrp3 = ucPrioFlagGrp2 % 8;
	ucPositionInGrp4 = ucPrioFlagGrp3;

	pstrPrioFlag->aucPrioFlagGrp1[ucPrioFlagGrp1] &= ~((U8) (1 << ucPositionInGrp1));
	if (0 == pstrPrioFlag->aucPrioFlagGrp1[ucPrioFlagGrp1]) {
		pstrPrioFlag->aucPrioFlagGrp2[ucPrioFlagGrp2] &= ~((U8) (1 << ucPositionInGrp2));
		if (0 == pstrPrioFlag->aucPrioFlagGrp2[ucPrioFlagGrp2]) {
			pstrPrioFlag->aucPrioFlagGrp3[ucPrioFlagGrp3] &= ~((U8) (1 << ucPositionInGrp3));
			if (0 == pstrPrioFlag->aucPrioFlagGrp3[ucPrioFlagGrp3]) {
				pstrPrioFlag->ucPrioFlagGrp4 &= ~((U8) (1 << ucPositionInGrp4));
			}
		}
	}
#elif PRIORITYNUM >= PRIORITY128
	ucPrioFlagGrp1 = ucTaskPrio / 8;
	ucPrioFlagGrp2 = ucPrioFlagGrp1 / 8;

	ucPositionInGrp1 = ucTaskPrio % 8;
	ucPositionInGrp2 = ucPrioFlagGrp1 % 8;
	ucPositionInGrp3 = ucPrioFlagGrp2;

	pstrPrioFlag->aucPrioFlagGrp1[ucPrioFlagGrp1] &= ~((U8) (1 << ucPositionInGrp1));
	if (0 == pstrPrioFlag->aucPrioFlagGrp1[ucPrioFlagGrp1]) {
		pstrPrioFlag->aucPrioFlagGrp2[ucPrioFlagGrp2] &= ~((U8) (1 << ucPositionInGrp2));
		if (0 == pstrPrioFlag->aucPrioFlagGrp2[ucPrioFlagGrp2]) {
			pstrPrioFlag->ucPrioFlagGrp3 &= ~((U8) (1 << ucPositionInGrp3));
		}
	}
#elif PRIORITYNUM >= PRIORITY16
	ucPrioFlagGrp1 = ucTaskPrio / 8;

	ucPositionInGrp1 = ucTaskPrio % 8;
	ucPositionInGrp2 = ucPrioFlagGrp1;

	pstrPrioFlag->aucPrioFlagGrp1[ucPrioFlagGrp1] &= ~((U8) (1 << ucPositionInGrp1));
	if (0 == pstrPrioFlag->aucPrioFlagGrp1[ucPrioFlagGrp1]) {
		pstrPrioFlag->ucPrioFlagGrp2 &= ~((U8) (1 << ucPositionInGrp2));
	}
#else
	pstrPrioFlag->ucPrioFlagGrp1 &= ~((U8) (1 << ucTaskPrio));
#endif
}

PRIORITYBITS REMIX_TaskGetHighestPrio(REMIX_PRIOFLAG * pstrPrioFlag)
{
#if PRIORITYNUM >= PRIORITY1024
	U8 ucPrioFlagGrp1;
	U8 ucPrioFlagGrp2;
	U8 ucPrioFlagGrp3;
	U8 ucHighestFlagInGrp1;
#elif PRIORITYNUM >= PRIORITY128
	U8 ucPrioFlagGrp1;
	U8 ucPrioFlagGrp2;
	U8 ucHighestFlagInGrp1;
#elif PRIORITYNUM >= PRIORITY16
	U8 ucPrioFlagGrp1;
	U8 ucHighestFlagInGrp1;
#endif

#ifdef REMIX_UNMAP

#if PRIORITYNUM >= PRIORITY1024
	ucPrioFlagGrp3 = caucTaskPrioUnmapTab[pstrPrioFlag->ucPrioFlagGrp4];
	ucPrioFlagGrp2 = caucTaskPrioUnmapTab[pstrPrioFlag->aucPrioFlagGrp3[ucPrioFlagGrp3]];
	ucPrioFlagGrp1 = caucTaskPrioUnmapTab[pstrPrioFlag->aucPrioFlagGrp2[ucPrioFlagGrp3 * 8 + ucPrioFlagGrp2]];
	ucHighestFlagInGrp1 =
	    caucTaskPrioUnmapTab[pstrPrioFlag->aucPrioFlagGrp1
				 [(ucPrioFlagGrp3 * 8 + ucPrioFlagGrp2) * 8 + ucPrioFlagGrp1]];
	return (PRIORITYBITS) (((ucPrioFlagGrp3 * 8 + ucPrioFlagGrp2) * 8 + ucPrioFlagGrp1) * 8 + ucHighestFlagInGrp1);
#elif PRIORITYNUM >= PRIORITY128
	ucPrioFlagGrp2 = caucTaskPrioUnmapTab[pstrPrioFlag->ucPrioFlagGrp3];
	ucPrioFlagGrp1 = caucTaskPrioUnmapTab[pstrPrioFlag->aucPrioFlagGrp2[ucPrioFlagGrp2]];
	ucHighestFlagInGrp1 = caucTaskPrioUnmapTab[pstrPrioFlag->aucPrioFlagGrp1[ucPrioFlagGrp2 * 8 + ucPrioFlagGrp1]];
	return (PRIORITYBITS) ((ucPrioFlagGrp2 * 8 + ucPrioFlagGrp1) * 8 + ucHighestFlagInGrp1);
#elif PRIORITYNUM >= PRIORITY16
	ucPrioFlagGrp1 = caucTaskPrioUnmapTab[pstrPrioFlag->ucPrioFlagGrp2];
	ucHighestFlagInGrp1 = caucTaskPrioUnmapTab[pstrPrioFlag->aucPrioFlagGrp1[ucPrioFlagGrp1]];
	return (PRIORITYBITS) (ucPrioFlagGrp1 * 8 + ucHighestFlagInGrp1);
#else
	return caucTaskPrioUnmapTab[pstrPrioFlag->ucPrioFlagGrp1];
#endif

#else

#if PRIORITYNUM >= PRIORITY1024
	ucPrioFlagGrp3 = REMIX_CalcPrioFromPrioFlag(pstrPrioFlag->ucPrioFlagGrp4);
	ucPrioFlagGrp2 = REMIX_CalcPrioFromPrioFlag(pstrPrioFlag->aucPrioFlagGrp3[ucPrioFlagGrp3]);
	ucPrioFlagGrp1 = REMIX_CalcPrioFromPrioFlag(pstrPrioFlag->aucPrioFlagGrp2[ucPrioFlagGrp3 * 8 + ucPrioFlagGrp2]);
	ucHighestFlagInGrp1 =
	    REMIX_CalcPrioFromPrioFlag(pstrPrioFlag->aucPrioFlagGrp1
				       [(ucPrioFlagGrp3 * 8 + ucPrioFlagGrp2) * 8 + ucPrioFlagGrp1]);
	return (PRIORITYBITS) (((ucPrioFlagGrp3 * 8 + ucPrioFlagGrp2) * 8 + ucPrioFlagGrp1) * 8 + ucHighestFlagInGrp1);
#elif PRIORITYNUM >= PRIORITY128
	ucPrioFlagGrp2 = REMIX_CalcPrioFromPrioFlag(pstrPrioFlag->ucPrioFlagGrp3);
	ucPrioFlagGrp1 = REMIX_CalcPrioFromPrioFlag(pstrPrioFlag->aucPrioFlagGrp2[ucPrioFlagGrp2]);
	ucHighestFlagInGrp1 =
	    REMIX_CalcPrioFromPrioFlag(pstrPrioFlag->aucPrioFlagGrp1[ucPrioFlagGrp2 * 8 + ucPrioFlagGrp1]);
	return (PRIORITYBITS) ((ucPrioFlagGrp2 * 8 + ucPrioFlagGrp1) * 8 + ucHighestFlagInGrp1);
#elif PRIORITYNUM >= PRIORITY16
	ucPrioFlagGrp1 = REMIX_CalcPrioFromPrioFlag(pstrPrioFlag->ucPrioFlagGrp2);
	ucHighestFlagInGrp1 = REMIX_CalcPrioFromPrioFlag(pstrPrioFlag->aucPrioFlagGrp1[ucPrioFlagGrp1]);
	return (PRIORITYBITS) (ucPrioFlagGrp1 * 8 + ucHighestFlagInGrp1);
#else
	return REMIX_CalcPrioFromPrioFlag(pstrPrioFlag->ucPrioFlagGrp1);
#endif

#endif
}

//*************************************************************************//

void REMIX_IdleTask(void *pvPara)
{
	while (1){
        REMIX_IdleHook();
    }
}

U32 REMIX_GetSystemTick(void)
{
	return guiTick;
}

REMIX_TCB *REMIX_GetCurrentTcb(void)
{
	return gpstrCurTcb;
}

REMIX_TCB *REMIX_GetRootTcb(void)
{
	return gpstrRootTaskTcb;
}

REMIX_TCB *REMIX_GetIdleTcb(void)
{
	return gpstrIdleTaskTcb;
}

void REMIX_SetUser(U32 uiUser)
{
	guiUser = uiUser;
}

U32 REMIX_GetUser(void)
{
	return guiUser;
}

U8 *REMIX_GetREMIXVersion(void)
{
	return REMIX_VERSION;
}

//*************************************************************************//

void REMIX_MemClr(U8 * pDest, U32 uiSize)
{
	while (uiSize > 0u) {
		*pDest++ = (U8) 0;
		uiSize--;
	}
}

void REMIX_MemCopy(U8 * pDest, U8 * pSrc, U32 uiSize)
{
	while (uiSize > 0u) {
		*pDest++ = *pSrc++;
		uiSize--;
	}
}
