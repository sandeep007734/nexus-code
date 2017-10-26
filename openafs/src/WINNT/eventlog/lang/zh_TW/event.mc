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
LanguageNames=(Chinese_Traditional=1:MSG000001)

;
;/* Test message text */
;

MessageId=0x0001
Severity=Informational
SymbolicName=AFSEVT_SVR_TEST_MSG_NOARGS
Language=Chinese_Traditional
AFS ���A���ƥ��x���հT���C
.

MessageId=
Severity=Warning
SymbolicName=AFSEVT_SVR_TEST_MSG_TWOARGS
Language=Chinese_Traditional
AFS ���A���ƥ��x���հT�� (str1: %1, str2: %2)�C
.



;
;/* General messages for all AFS server processes */
;

MessageId=0x0101
Severity=Error
SymbolicName=AFSEVT_SVR_FAILED_ASSERT
Language=Chinese_Traditional
AFS ���A���B�z���ĽT�{�G�b�ɮ� %2 ���� %1 ��C
.

MessageId=
Severity=Error
SymbolicName=AFSEVT_SVR_NO_INSTALL_DIR
Language=Chinese_Traditional
%1 �L�k��� AFS �n��w�˥ؿ��C
.

MessageId=
Severity=Error
SymbolicName=AFSEVT_SVR_WINSOCK_INIT_FAILED
Language=Chinese_Traditional
%1 �L�k�_�l�]�w Windows Sockets �{���w�C
.



;
;/* AFS BOS control (startup/shutdown) service messages */
;

MessageId=0x0201
Severity=Informational
SymbolicName=AFSEVT_SVR_BCS_STARTED
Language=Chinese_Traditional
�w�Ұ� AFS BOS ����A�ȡC
.

MessageId=
Severity=Informational
SymbolicName=AFSEVT_SVR_BCS_STOPPED
Language=Chinese_Traditional
�w���� AFS BOS ����A�ȡC
.

MessageId=
Severity=Error
SymbolicName=AFSEVT_SVR_BCS_SCM_COMM_FAILED
Language=Chinese_Traditional
AFS BOS ����A�ȵL�k�P�t�� SCM �q�H�C
.

MessageId=
Severity=Error
SymbolicName=AFSEVT_SVR_BCS_HANDLER_REG_FAILED
Language=Chinese_Traditional
AFS BOS ����A�ȵL�k�n���ƥ�B�z�`���CAFS ���A���n�骺�[�c�i�ण���T�C
.

MessageId=
Severity=Error
SymbolicName=AFSEVT_SVR_BCS_INSUFFICIENT_RESOURCES
Language=Chinese_Traditional
AFS BOS ����A�ȵL�k���o���n���t�θ귽�C
.

MessageId=
Severity=Error
SymbolicName=AFSEVT_SVR_BCS_INTERNAL_ERROR
Language=Chinese_Traditional
AFS BOS ����A�ȵo�ͤ������~�C
.

MessageId=
Severity=Error
SymbolicName=AFSEVT_SVR_BCS_NO_INSTALL_DIR
Language=Chinese_Traditional
AFS BOS ����A�ȵL�k��� AFS �n��w�˥ؿ��CAFS ���A���n�骺�[�c�i�ण���T�C
.

MessageId=
Severity=Error
SymbolicName=AFSEVT_SVR_BCS_BOSSERVER_START_FAILED
Language=Chinese_Traditional
AFS BOS ����A�ȵL�k�Ұʩέ��s�Ұ� AFS �D���A���C
.

MessageId=
Severity=Error
SymbolicName=AFSEVT_SVR_BCS_BOSSERVER_STOP_FAILED
Language=Chinese_Traditional
AFS BOS ����A�ȵL�k���� AFS �D���A���C�Ҧ��� AFS ���A���B�z�������H��ʤ覡����
�]�յ۳z�L fskill ���O�ǰe SIGQUIT �T���� AFS �D���A���^�C
.

MessageId=
Severity=Warning
SymbolicName=AFSEVT_SVR_BCS_BOSSERVER_STOP_TIMEOUT
Language=Chinese_Traditional
AFS BOS ����A�ȩ�󵥭� AFS �D���A������C���ˬd�Ҧ� AFS ���A���B�z���w�b���s�ҰʪA�Ȥ��e����C
.

MessageId=
Severity=Informational
SymbolicName=AFSEVT_SVR_BCS_BOSSERVER_RESTART
Language=Chinese_Traditional
AFS BOS ����A�ȥ��b���s�Ұ� AFS �D���A���C
.

MessageId=
Severity=Informational
SymbolicName=AFSEVT_SVR_BCS_BOSSERVER_EXIT
Language=Chinese_Traditional
AFS BOS ����A�Ȱ����� AFS �D���A�������A�åB���n�D���s�ҰʡC
.



;
;#endif /* OPENAFS_AFSEVENT_H */
