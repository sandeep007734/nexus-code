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


LanguageNames=(Japanese=1:MSG000001)
MessageIdTypedef=unsigned

;
;/* Test message text */
;

MessageId=0x0001
Severity=Informational
SymbolicName=AFSEVT_SVR_TEST_MSG_NOARGS
Language=Japanese
AFS �T�[�o�[�E�C�x���g�E���O�E�e�X�g�E���b�Z�[�W�B
.

MessageId=
Severity=Warning
SymbolicName=AFSEVT_SVR_TEST_MSG_TWOARGS
Language=Japanese
AFS �T�[�o�[�E�C�x���g�E���O�E�e�X�g�E���b�Z�[�W (str1: %1, str2: %2)�B
.



;
;/* General messages for all AFS server processes */
;

MessageId=0x0101
Severity=Error
SymbolicName=AFSEVT_SVR_FAILED_ASSERT
Language=Japanese
AFS �T�[�o�[�E�v���Z�X������Ɏ��s���܂���: �s %1 �t�@�C�� %2�B
.

MessageId=
Severity=Error
SymbolicName=AFSEVT_SVR_NO_INSTALL_DIR
Language=Japanese
%1 �� AFS �\�t�g�E�F�A�̃C���X�g�[���E�f�B���N�g���[���������܂���ł����B
.

MessageId=
Severity=Error
SymbolicName=AFSEVT_SVR_WINSOCK_INIT_FAILED
Language=Japanese
%1 �� Windows Socket ���C�u�����[���������ł��܂���ł����B
.



;
;/* AFS BOS control (startup/shutdown) service messages */
;

MessageId=0x0201
Severity=Informational
SymbolicName=AFSEVT_SVR_BCS_STARTED
Language=Japanese
AFS BOS ����T�[�r�X���n�����܂����B
.

MessageId=
Severity=Informational
SymbolicName=AFSEVT_SVR_BCS_STOPPED
Language=Japanese
AFS BOS ����T�[�r�X����~���܂����B
.

MessageId=
Severity=Error
SymbolicName=AFSEVT_SVR_BCS_SCM_COMM_FAILED
Language=Japanese
AFS BOS ����T�[�r�X���V�X�e�� SCM �ƒʐM�ł��܂���B
.

MessageId=
Severity=Error
SymbolicName=AFSEVT_SVR_BCS_HANDLER_REG_FAILED
Language=Japanese
AFS BOS ����T�[�r�X���C�x���g�E�n���h���[��o�^�ł��܂���BAFS �T�[�o�[�E�\�t�g�E�F�A���������\������Ă��Ȃ��\��������܂��B
.

MessageId=
Severity=Error
SymbolicName=AFSEVT_SVR_BCS_INSUFFICIENT_RESOURCES
Language=Japanese
AFS BOS ����T�[�r�X���K�v�ȃV�X�e���E���\�[�X���擾�ł��܂���B
.

MessageId=
Severity=Error
SymbolicName=AFSEVT_SVR_BCS_INTERNAL_ERROR
Language=Japanese
AFS BOS ����T�[�r�X�������G���[�����o���܂����B
.

MessageId=
Severity=Error
SymbolicName=AFSEVT_SVR_BCS_NO_INSTALL_DIR
Language=Japanese
AFS BOS ����T�[�r�X�� AFS �\�t�g�E�F�A�̃C���X�g�[���E�f�B���N�g���[���������܂���ł����BAFS �T�[�o�[�E�\�t�g�E�F�A���������\������Ă��Ȃ��\��������܂��B
.

MessageId=
Severity=Error
SymbolicName=AFSEVT_SVR_BCS_BOSSERVER_START_FAILED
Language=Japanese
AFS BOS ����T�[�r�X�� AFS bosserver ���n���܂��͍Ďn���ł��܂���ł����B
.

MessageId=
Severity=Error
SymbolicName=AFSEVT_SVR_BCS_BOSSERVER_STOP_FAILED
Language=Japanese
AFS BOS ����T�[�r�X�� AFS bosserver ���~�ł��܂���ł����BAFS �T�[�o�[�E�v���Z�X�����ׂĎ蓮�Œ�~����K�v������܂� (AFS bosserver �� afskill �R�}���h�� SIGQUIT �V�O�i���𑗐M���Ă݂Ă�������)�B
.

MessageId=
Severity=Warning
SymbolicName=AFSEVT_SVR_BCS_BOSSERVER_STOP_TIMEOUT
Language=Japanese
AFS BOS ����T�[�r�X�� AFS bosserver �̒�~�҂��𒆎~���܂����B�T�[�r�X���Ďn������O�ɁA���ׂĂ� AFS �T�[�o�[�E�v���Z�X����~���Ă��邱�Ƃ��m���߂Ă��������B
.

MessageId=
Severity=Informational
SymbolicName=AFSEVT_SVR_BCS_BOSSERVER_RESTART
Language=Japanese
AFS BOS ����T�[�r�X�� AFS bosserver ���Ďn�����Ă��܂��B
.

MessageId=
Severity=Informational
SymbolicName=AFSEVT_SVR_BCS_BOSSERVER_EXIT
Language=Japanese
AFS BOS ����T�[�r�X���AAFS bosserver ���Ďn���v���Ȃ��ŏI���������Ƃ����o���܂����B
.



;
;#endif /* OPENAFS_AFSEVENT_H */
