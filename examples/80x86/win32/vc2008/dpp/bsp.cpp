//////////////////////////////////////////////////////////////////////////////
// Product: DPP example, Win32
// Last Updated for Version: 4.3.00
// Date of the Last Update:  Dec 24, 2011
//
//                    Q u a n t u m     L e a P s
//                    ---------------------------
//                    innovating embedded systems
//
// Copyright (C) 2002-2011 Quantum Leaps, LLC. All rights reserved.
//
// This software may be distributed and modified under the terms of the GNU
// General Public License version 2 (GPL) as published by the Free Software
// Foundation and appearing in the file GPL.TXT included in the packaging of
// this file. Please note that GPL Section 2[b] requires that all works based
// on this software must also be made publicly available under the terms of
// the GPL ("Copyleft").
//
// Alternatively, this software may be distributed and modified under the
// terms of Quantum Leaps commercial licenses, which expressly supersede
// the GPL and are specifically designed for licensees interested in
// retaining the proprietary status of their code.
//
// Contact information:
// Quantum Leaps Web site:  http://www.quantum-leaps.com
// e-mail:                  info@quantum-leaps.com
//////////////////////////////////////////////////////////////////////////////
#include "qp_port.h"
#include "dpp.h"
#include "bsp.h"

#include <conio.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>

Q_DEFINE_THIS_FILE

// local variables -----------------------------------------------------------
static uint8_t l_running;

#ifdef Q_SPY

enum QSUserRecords {
    PHILO_STAT = QS_USER
};

static SOCKET l_sock = INVALID_SOCKET;
#endif

//............................................................................
static DWORD WINAPI idleThread(LPVOID par) {   // signature for CreateThread()
    (void)par;
    l_running = (uint8_t)1;
    while (l_running) {
        Sleep(10);                                         // wait for a while
        if (_kbhit()) {                                    // any key pressed?
            if (_getch() == '\33') {             // see if the ESC key pressed
                QF::PUBLISH(Q_NEW(QEvent, TERMINATE_SIG), (void *)0);
            }
        }
#ifdef Q_SPY
        {
            uint16_t nBytes = 1024;
            uint8_t const *block;
            QF_CRIT_ENTRY(dummy);
            block = QS::getBlock(&nBytes);
            QF_CRIT_EXIT(dummy);
            if (block != (uint8_t *)0) {
                send(l_sock, (char const *)block, nBytes, 0);
            }
        }
#endif
    }
    return 0;                                                // return success
}
//............................................................................
void BSP_init(int argc, char *argv[]) {
    HANDLE hIdle;
    char const *hostAndPort = "localhost:6601";

    if (argc > 1) {                                         // port specified?
        hostAndPort = argv[1];
    }
    if (!QS_INIT(hostAndPort)) {
        printf("\nUnable to open QSpy socket\n");
        exit(-1);
    }

    QF_setTickRate(BSP_TICKS_PER_SEC);            // set the desired tick rate

    hIdle = CreateThread(NULL, 1024, &idleThread, (void *)0, 0, NULL);
    Q_ASSERT(hIdle != (HANDLE)0);                    // thread must be created
    SetThreadPriority(hIdle, THREAD_PRIORITY_IDLE);

    printf("Dining Philosopher Problem example"
           "\nQEP %s\nQF  %s\n"
           "Press ESC to quit...\n",
           QEP::getVersion(),
           QF::getVersion());
}
//............................................................................
void QF::onStartup(void) {
}
//............................................................................
void QF::onCleanup(void) {
    l_running = (uint8_t)0;                            // stop the idle thread
}
//............................................................................
void BSP_displyPhilStat(uint8_t n, char const *stat) {
    printf("Philosopher %2d is %s\n", (int)n, stat);

    QS_BEGIN(PHILO_STAT, AO_Philo[n])     // application-specific record begin
        QS_U8(1, n);                                     // Philosopher number
        QS_STR(stat);                                    // Philosopher status
    QS_END()
}
//............................................................................
void Q_onAssert(char const Q_ROM * const Q_ROM_VAR file, int line) {
    fprintf(stderr, "Assertion failed in %s, line %d", file, line);
    exit(0);
}

//----------------------------------------------------------------------------
#ifdef Q_SPY                                            // define QS callbacks

uint8_t QS::onStartup(void const *arg) {
    static uint8_t qsBuf[1024];                   // 1K buffer for Quantum Spy
    static WSADATA wsaData;
    char host[64];
    char const *src;
    char *dst;
    USHORT port = 6601;                                        // default port
    ULONG ioctl_opt = 1;
    struct sockaddr_in servAddr;
    struct hostent *server;

    initBuf(qsBuf, sizeof(qsBuf));

       // initialize Windows sockets */
    if (WSAStartup(MAKEWORD(2,0), &wsaData) == SOCKET_ERROR) {
        printf("Windows Sockets cannot be initialized.");
        return (uint8_t)0;
    }

    src = (char const *)arg;
    dst = host;
    while ((*src != '\0') && (*src != ':') && (dst < &host[sizeof(host)])) {
        *dst++ = *src++;
    }
    *dst = '\0';
    if (*src == ':') {
        port = (USHORT)strtoul(src + 1, NULL, 10);
    }


    l_sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);          // TCP socket
    if (l_sock == INVALID_SOCKET){
        printf("Socket cannot be created.\n"
               "Windows socket error 0x%08X.",
               WSAGetLastError());
        return (uint8_t)0;
    }

    server = gethostbyname(host);
    if (server == NULL) {
        printf("QSpy host name cannot be resolved.\n"
               "Windows socket error 0x%08X.",
               WSAGetLastError());
        return (uint8_t)0;
    }
    memset(&servAddr, 0, sizeof(servAddr));
    servAddr.sin_family = AF_INET;
    memcpy(&servAddr.sin_addr, server->h_addr, server->h_length);
    servAddr.sin_port = htons(port);
    if (connect(l_sock, (struct sockaddr *)&servAddr, sizeof(servAddr))
        == SOCKET_ERROR)
    {
        printf("Socket cannot be connected to the QSpy server.\n"
               "Windows socket error 0x%08X.",
               WSAGetLastError());
        QS_EXIT();
        return (uint8_t)0;
    }

       // Set the socket to non-blocking mode. */
    if (ioctlsocket(l_sock, FIONBIO, &ioctl_opt) == SOCKET_ERROR) {
        printf("Socket configuration failed.\n"
               "Windows socket error 0x%08X.",
               WSAGetLastError());
        QS_EXIT();
        return (uint8_t)0;
    }
                                                    // setup the QS filters...
    QS_FILTER_ON(QS_ALL_RECORDS);

//    QS_FILTER_OFF(QS_QEP_STATE_EMPTY);
//    QS_FILTER_OFF(QS_QEP_STATE_ENTRY);
//    QS_FILTER_OFF(QS_QEP_STATE_EXIT);
//    QS_FILTER_OFF(QS_QEP_STATE_INIT);
//    QS_FILTER_OFF(QS_QEP_INIT_TRAN);
//    QS_FILTER_OFF(QS_QEP_INTERN_TRAN);
//    QS_FILTER_OFF(QS_QEP_TRAN);
//    QS_FILTER_OFF(QS_QEP_IGNORED);

    QS_FILTER_OFF(QS_QF_ACTIVE_ADD);
    QS_FILTER_OFF(QS_QF_ACTIVE_REMOVE);
    QS_FILTER_OFF(QS_QF_ACTIVE_SUBSCRIBE);
//    QS_FILTER_OFF(QS_QF_ACTIVE_UNSUBSCRIBE);
    QS_FILTER_OFF(QS_QF_ACTIVE_POST_FIFO);
//    QS_FILTER_OFF(QS_QF_ACTIVE_POST_LIFO);
    QS_FILTER_OFF(QS_QF_ACTIVE_GET);
    QS_FILTER_OFF(QS_QF_ACTIVE_GET_LAST);
    QS_FILTER_OFF(QS_QF_EQUEUE_INIT);
    QS_FILTER_OFF(QS_QF_EQUEUE_POST_FIFO);
    QS_FILTER_OFF(QS_QF_EQUEUE_POST_LIFO);
    QS_FILTER_OFF(QS_QF_EQUEUE_GET);
    QS_FILTER_OFF(QS_QF_EQUEUE_GET_LAST);
    QS_FILTER_OFF(QS_QF_MPOOL_INIT);
    QS_FILTER_OFF(QS_QF_MPOOL_GET);
    QS_FILTER_OFF(QS_QF_MPOOL_PUT);
    QS_FILTER_OFF(QS_QF_PUBLISH);
    QS_FILTER_OFF(QS_QF_NEW);
    QS_FILTER_OFF(QS_QF_GC_ATTEMPT);
    QS_FILTER_OFF(QS_QF_GC);
//    QS_FILTER_OFF(QS_QF_TICK);
    QS_FILTER_OFF(QS_QF_TIMEEVT_ARM);
    QS_FILTER_OFF(QS_QF_TIMEEVT_AUTO_DISARM);
    QS_FILTER_OFF(QS_QF_TIMEEVT_DISARM_ATTEMPT);
    QS_FILTER_OFF(QS_QF_TIMEEVT_DISARM);
    QS_FILTER_OFF(QS_QF_TIMEEVT_REARM);
    QS_FILTER_OFF(QS_QF_TIMEEVT_POST);
    QS_FILTER_OFF(QS_QF_CRIT_ENTRY);
    QS_FILTER_OFF(QS_QF_CRIT_EXIT);
    QS_FILTER_OFF(QS_QF_ISR_ENTRY);
    QS_FILTER_OFF(QS_QF_ISR_EXIT);

    return (uint8_t)1;                                              // success
}
//............................................................................
void QS::onCleanup(void) {
    if (l_sock != INVALID_SOCKET) {
        closesocket(l_sock);
    }
    WSACleanup();
}
//............................................................................
void QS::onFlush(void) {
    uint16_t nBytes = 1000;
    uint8_t const *block;
    while ((block = getBlock(&nBytes)) != (uint8_t *)0) {
        send(l_sock, (char const *)block, nBytes, 0);
        nBytes = 1000;
    }
}
//............................................................................
QSTimeCtr QS::onGetTime(void) {
    return (QSTimeCtr)clock();
}
#endif                                                                // Q_SPY
//----------------------------------------------------------------------------


