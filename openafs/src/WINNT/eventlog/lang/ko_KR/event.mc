;/* Copyright 2000, International Business Machines Corporation and others.
; * All Rights Reserved.
; *
; * This software has been released under the terms of the IBM Public
; * License.  For details, see the LICENSE file in the top-level source
; * directory or online at http://www.openafs.org/dl/license10.html
; * event.mc --(mc)--> event.[h|rc] --(logevent.h + event.h)--> afsevent.h
; */
;
;#ifndef OPENAFS_AFSEVENT_H
;#define OPENAFS_AFSEVENT_H
;
;
;/* AFS event.mc format.
; *
; * AFS event messages are grouped by category.  The MessageId of the
; * first message in a given category specifies the starting identifier
; * range for that category; the second and later messages in a category
; * do NOT specify a MessageId value and thus receive the value of the
; * previous message plus one.
; *
; * To add a new message to an existing category, append it to the end of
; * that category.  To create a new category, provide an appropriate
; * comment line and specify a non-conflicting MessageId for the first
; * message in the new category.
; */
;


MessageIdTypedef=unsigned
LanguageNames=(Korean=1:MSG000001)

;
;/* Test message text */
;

MessageId=0x0001
Severity=Informational
SymbolicName=AFSEVT_SVR_TEST_MSG_NOARGS
Language=Korean
AFS ���� �̺�Ʈ �α� �˻� �޽���.
.

MessageId=
Severity=Warning
SymbolicName=AFSEVT_SVR_TEST_MSG_TWOARGS
Language=Korean
AFS ���� �̺�Ʈ �α� �˻� �޽���(str1: %1, str2: %2).
.



;
;/* General messages for all AFS server processes */
;

MessageId=0x0101
Severity=Error
SymbolicName=AFSEVT_SVR_FAILED_ASSERT
Language=Korean
AFS ���� ���μ����� ���ο� �����߽��ϴ�: %2 ������ %1 ��.
.

MessageId=
Severity=Error
SymbolicName=AFSEVT_SVR_NO_INSTALL_DIR
Language=Korean
%1��(��) AFS ����Ʈ���� ��ġ ���丮�� ã�� ���߽��ϴ�.
.

MessageId=
Severity=Error
SymbolicName=AFSEVT_SVR_WINSOCK_INIT_FAILED
Language=Korean
%1��(��) Windows ���� ���̺귯���� �ʱ�ȭ���� ���߽��ϴ�.
.



;
;/* AFS BOS control (startup/shutdown) service messages */
;

MessageId=0x0201
Severity=Informational
SymbolicName=AFSEVT_SVR_BCS_STARTED
Language=Korean
AFS BOS ���� ���񽺰� ���۵Ǿ����ϴ�.
.

MessageId=
Severity=Informational
SymbolicName=AFSEVT_SVR_BCS_STOPPED
Language=Korean
AFS BOS ���� ���񽺰� �����Ǿ����ϴ�.
.

MessageId=
Severity=Error
SymbolicName=AFSEVT_SVR_BCS_SCM_COMM_FAILED
Language=Korean
AFS BOS ���� ���񽺰� SCM �ý��۰� ����� �� �����ϴ�.
.

MessageId=
Severity=Error
SymbolicName=AFSEVT_SVR_BCS_HANDLER_REG_FAILED
Language=Korean
AFS BOS ���� ���񽺰� �̺�Ʈ ó���⸦ ����� �� �����ϴ�.
AFS ���� ����Ʈ��� ����� �������� ���� ���� ���� �ֽ��ϴ�.
.

MessageId=
Severity=Error
SymbolicName=AFSEVT_SVR_BCS_INSUFFICIENT_RESOURCES
Language=Korean
AFS BOS ���� ���񽺰� �ʿ��� �ý��� �ڿ��� ���� �� �����ϴ�.
.

MessageId=
Severity=Error
SymbolicName=AFSEVT_SVR_BCS_INTERNAL_ERROR
Language=Korean
AFS BOS ���� ���񽺿� ���� ������ �߻��߽��ϴ�.
.

MessageId=
Severity=Error
SymbolicName=AFSEVT_SVR_BCS_NO_INSTALL_DIR
Language=Korean
AFS BOS ���� ���񽺰� AFS ����Ʈ���� ��ġ ���丮�� ã�� ���߽��ϴ�.
AFS ���� ����Ʈ��� ����� �������� ���� ���� ���� �ֽ��ϴ�.
.

MessageId=
Severity=Error
SymbolicName=AFSEVT_SVR_BCS_BOSSERVER_START_FAILED
Language=Korean
AFS BOS ���� ���񽺰� AFS bosserver�� ���� �Ǵ� ��������� ���߽��ϴ�.
.

MessageId=
Severity=Error
SymbolicName=AFSEVT_SVR_BCS_BOSSERVER_STOP_FAILED
Language=Korean
AFS BOS ���� ���񽺰� AFS bosserver�� �������� ���߽��ϴ�.
��� AFS ���� ���μ����� �������� �������Ѿ� �մϴ�(afskill ����� ���� SIGQUIT ��ȣ�� AFS bosserver�� ���� ���ʽÿ�).
.

MessageId=
Severity=Warning
SymbolicName=AFSEVT_SVR_BCS_BOSSERVER_STOP_TIMEOUT
Language=Korean
AFS BOS ���� ���񽺰� AFS bosserver�� ������ ������ ��ٸ��� ���� �����߽��ϴ�.
��� AFS ���� ���μ����� ���񽺸� ������ϱ� ���� �����Ǿ����� Ȯ���Ͻʽÿ�.
.

MessageId=
Severity=Informational
SymbolicName=AFSEVT_SVR_BCS_BOSSERVER_RESTART
Language=Korean
AFS BOS ���� ���񽺰� AFS bosserver�� ������ϰ� �ֽ��ϴ�.
.

MessageId=
Severity=Informational
SymbolicName=AFSEVT_SVR_BCS_BOSSERVER_EXIT
Language=Korean
AFS BOS ���� ���񽺰� AFS bosserver�� ������� ��û���� �ʰ� ����Ǿ����� �����߽��ϴ�.
.



;
;#endif /* OPENAFS_AFSEVENT_H */
