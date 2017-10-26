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

;
;/* Test message text */
;

MessageId=0x0001
Severity=Informational
SymbolicName=AFSEVT_SVR_TEST_MSG_NOARGS
Language=English
AFS �������¼���־������Ϣ��
.

MessageId=
Severity=Warning
SymbolicName=AFSEVT_SVR_TEST_MSG_TWOARGS
Language=English
AFS �������¼���־������Ϣ(str1: %1�� str2: %2)��
.



;
;/* General messages for all AFS server processes */
;

MessageId=0x0101
Severity=Error
SymbolicName=AFSEVT_SVR_FAILED_ASSERT
Language=English
AFS ����������ʧ�ܣ��ļ� %2 ���� %1��
.

MessageId=
Severity=Error
SymbolicName=AFSEVT_SVR_NO_INSTALL_DIR
Language=English
%1 ��λ AFS �����װĿ¼ʧ�ܡ�
.

MessageId=
Severity=Error
SymbolicName=AFSEVT_SVR_WINSOCK_INIT_FAILED
Language=English
%1 ��ʼ�� Windows �׽��ֿ�ʧ�ܡ�
.



;
;/* AFS BOS control (startup/shutdown) service messages */
;

MessageId=0x0201
Severity=Informational
SymbolicName=AFSEVT_SVR_BCS_STARTED
Language=English
AFS BOS ���Ʒ�����������
.

MessageId=
Severity=Informational
SymbolicName=AFSEVT_SVR_BCS_STOPPED
Language=English
AFS BOS ���Ʒ�����ֹͣ��
.

MessageId=
Severity=Error
SymbolicName=AFSEVT_SVR_BCS_SCM_COMM_FAILED
Language=English
AFS BOS ���Ʒ����޷���ϵͳ SCM ͨ�š�
.

MessageId=
Severity=Error
SymbolicName=AFSEVT_SVR_BCS_HANDLER_REG_FAILED
Language=English
AFS BOS ���Ʒ����޷�ע���¼���������
AFS �������������δ��ȷ���á�
.

MessageId=
Severity=Error
SymbolicName=AFSEVT_SVR_BCS_INSUFFICIENT_RESOURCES
Language=English
AFS BOS ���Ʒ����޷���ñ����ϵͳ��Դ��
.

MessageId=
Severity=Error
SymbolicName=AFSEVT_SVR_BCS_INTERNAL_ERROR
Language=English
AFS BOS ���Ʒ�����һ���ڲ�����
.

MessageId=
Severity=Error
SymbolicName=AFSEVT_SVR_BCS_NO_INSTALL_DIR
Language=English
AFS BOS ���Ʒ���λ AFS �����װĿ¼ʧ�ܡ�AFS �������������δ��ȷ���á�
.

MessageId=
Severity=Error
SymbolicName=AFSEVT_SVR_BCS_BOSSERVER_START_FAILED
Language=English
AFS BOS ���Ʒ����������������� AFS bosserver ʧ�ܡ�
.

MessageId=
Severity=Error
SymbolicName=AFSEVT_SVR_BCS_BOSSERVER_STOP_FAILED
Language=English
AFS BOS ���Ʒ���ֹͣ AFS bosserver ʧ�ܡ����� AFS ���������̱���
�ֹ�ֹͣ(ͨ�� afskill ������ AFS bosserver ���� SIGQUIT �ź�)��
.

MessageId=
Severity=Warning
SymbolicName=AFSEVT_SVR_BCS_BOSSERVER_STOP_TIMEOUT
Language=English
AFS BOS ���Ʒ�������ȴ� AFS bosserver ��ֹͣ����������������ǰ������� AFS �����������Ƿ���ֹͣ��
.

MessageId=
Severity=Informational
SymbolicName=AFSEVT_SVR_BCS_BOSSERVER_RESTART
Language=English
AFS BOS ���Ʒ��������������� AFS bosserver��
.

MessageId=
Severity=Informational
SymbolicName=AFSEVT_SVR_BCS_BOSSERVER_EXIT
Language=English
AFS BOS ���Ʒ�����⵽ AFS bosserver û�������������������˳���
.



;
;#endif /* OPENAFS_AFSEVENT_H */
