all_include_dirs = -g\
	-I$(top_srcdir)/src/lib
bin_PROGRAMS = \
	fqm\
	MakeTestData\
	ManageFQ\
	ManageSHMQ\
	TestFQ\
	TestSQ\
	deFQ_utl\
	deFQ_batch\
	deFQ_thread\
	deFQ_XA\
	deFQ_XA_thread\
	enFQ_utl\
	enFQ_thread\
	mem_queue_test\
	qvf\
	qvf_all\
	retrieveMon\
	ManageHashMap\
	SetQueueHeader\
	FatalNotifier\
	SetMaster\
	L4_listener\
	LogCollector\
	LogAgent\
	net_text_qt\
	FQ_server\
	GenQinfo\
	TCP_sync_server\
	TCP_sync_client\
	FQ_Protocol_svr\
	FQ_Protocol_cli_txt\
	relay_FQ\
	LogNotifier\
	enFQ_datafile\
	deQ_XA_enQ\
	lock_perfermance_TPS\
	deFQ_bundle\
	mv_file_to_fq\
	mv_file_b_to_fq\
	mv_file_b_to_fq_multi\
	create_hash_map\
	unlink_hash_map\
	fq_snapshot\
	fq_view
#
if HAVE_SOCKET
all_libraries = -lm -ldl -lnsl -lrt -lpthread -lsocket -lm
else
all_libraries = -lm -ldl -lrt -lpthread -lm
endif

fqm_SOURCES = fqm.c
fqm_CFLAGS = $(all_include_dirs)
fqm_LDADD = $(all_libraries) ./../lib/libfq.la
#
MakeTestData_SOURCES = MakeTestData.c
MakeTestData_CFLAGS = $(all_include_dirs)
MakeTestData_LDADD = $(all_libraries) ./../lib/libfq.la
#
ManageFQ_SOURCES = ManageFQ.c
ManageFQ_CFLAGS = $(all_include_dirs)
ManageFQ_LDADD = $(all_libraries) ./../lib/libfq.la
#
ManageSHMQ_SOURCES = ManageSHMQ.c
ManageSHMQ_CFLAGS = $(all_include_dirs)
ManageSHMQ_LDADD = $(all_libraries) ./../lib/libfq.la
#
TestFQ_SOURCES = TestFQ.c
TestFQ_CFLAGS = $(all_include_dirs)
TestFQ_LDADD = $(all_libraries) ./../lib/libfq.la
#
TestSQ_SOURCES = TestSQ.c
TestSQ_CFLAGS = $(all_include_dirs)
TestSQ_LDADD = $(all_libraries) ./../lib/libfq.la
#
deFQ_utl_SOURCES = deFQ_utl.c
deFQ_utl_CFLAGS = $(all_include_dirs)
deFQ_utl_LDADD = $(all_libraries) ./../lib/libfq.la
#
deFQ_batch_SOURCES = deFQ_batch.c
deFQ_batch_CFLAGS = $(all_include_dirs)
deFQ_batch_LDADD = $(all_libraries) ./../lib/libfq.la
#
deFQ_thread_SOURCES = deFQ_thread.c
deFQ_thread_CFLAGS = $(all_include_dirs)
deFQ_thread_LDADD = $(all_libraries) ./../lib/libfq.la
#
deFQ_XA_SOURCES = deFQ_XA.c
deFQ_XA_CFLAGS = $(all_include_dirs)
deFQ_XA_LDADD = $(all_libraries) ./../lib/libfq.la
#
deFQ_XA_thread_SOURCES = deFQ_XA_thread.c
deFQ_XA_thread_CFLAGS = $(all_include_dirs)
deFQ_XA_thread_LDADD = $(all_libraries) ./../lib/libfq.la
#
enFQ_utl_SOURCES = enFQ_utl.c
enFQ_utl_CFLAGS = $(all_include_dirs)
enFQ_utl_LDADD = $(all_libraries) ./../lib/libfq.la
#
enFQ_thread_SOURCES = enFQ_thread.c
enFQ_thread_CFLAGS = $(all_include_dirs)
enFQ_thread_LDADD = $(all_libraries) ./../lib/libfq.la
#
mem_queue_test_SOURCES = mem_queue_test.c
mem_queue_test_CFLAGS = $(all_include_dirs)
mem_queue_test_LDADD = $(all_libraries) ./../lib/libfq.la
#
qvf_SOURCES = qvf.c
qvf_CFLAGS = $(all_include_dirs)
qvf_LDADD = $(all_libraries) ./../lib/libfq.la
#
qvf_all_SOURCES = qvf_all.c
qvf_all_CFLAGS = $(all_include_dirs)
qvf_all_LDADD = $(all_libraries) ./../lib/libfq.la
#
retrieveMon_SOURCES = retrieveMon.c
retrieveMon_CFLAGS = $(all_include_dirs)
retrieveMon_LDADD = $(all_libraries) ./../lib/libfq.la
#
ManageHashMap_SOURCES = ManageHashMap.c
ManageHashMap_CFLAGS = $(all_include_dirs)
ManageHashMap_LDADD = $(all_libraries) ./../lib/libfq.la
#
SetQueueHeader_SOURCES = SetQueueHeader.c
SetQueueHeader_CFLAGS = $(all_include_dirs)
SetQueueHeader_LDADD = $(all_libraries) ./../lib/libfq.la
#
FatalNotifier_SOURCES = FatalNotifier.c
FatalNotifier_CFLAGS = $(all_include_dirs)
FatalNotifier_LDADD = $(all_libraries) ./../lib/libfq.la
#
SetMaster_SOURCES = SetMaster.c
SetMaster_CFLAGS = $(all_include_dirs)
SetMaster_LDADD = $(all_libraries) ./../lib/libfq.la
#
L4_listener_SOURCES = L4_listener.c
L4_listener_CFLAGS = $(all_include_dirs)
L4_listener_LDADD = $(all_libraries) ./../lib/libfq.la
#
LogCollector_SOURCES = LogCollector.c
LogCollector_CFLAGS = $(all_include_dirs)
LogCollector_LDADD = $(all_libraries) ./../lib/libfq.la
#
LogAgent_SOURCES = LogAgent.c
LogAgent_CFLAGS = $(all_include_dirs)
LogAgent_LDADD = $(all_libraries) ./../lib/libfq.la
#
net_text_qt_SOURCES = net_text_qt.c
net_text_qt_CFLAGS = $(all_include_dirs)
net_text_qt_LDADD = $(all_libraries) ./../lib/libfq.la
#
FQ_server_SOURCES = FQ_server.c
FQ_server_CFLAGS = $(all_include_dirs)
FQ_server_LDADD = $(all_libraries) ./../lib/libfq.la
#
GenQinfo_SOURCES = GenQinfo.c
GenQinfo_CFLAGS = $(all_include_dirs)
GenQinfo_LDADD = $(all_libraries) ./../lib/libfq.la
#
TCP_sync_server_SOURCES = TCP_sync_server.c
TCP_sync_server_CFLAGS = $(all_include_dirs)
TCP_sync_server_LDADD = $(all_libraries) ./../lib/libfq.la
#
TCP_sync_client_SOURCES = TCP_sync_client.c
TCP_sync_client_CFLAGS = $(all_include_dirs)
TCP_sync_client_LDADD = $(all_libraries) ./../lib/libfq.la
#
FQ_Protocol_svr_SOURCES = FQ_Protocol_svr.c
FQ_Protocol_svr_CFLAGS = $(all_include_dirs)
FQ_Protocol_svr_LDADD = $(all_libraries) ./../lib/libfq.la
#
FQ_Protocol_cli_txt_SOURCES = FQ_Protocol_cli_txt.c
FQ_Protocol_cli_txt_CFLAGS = $(all_include_dirs)
FQ_Protocol_cli_txt_LDADD = $(all_libraries) ./../lib/libfq.la
#
relay_FQ_SOURCES = relay_FQ.c
relay_FQ_CFLAGS = $(all_include_dirs)
relay_FQ_LDADD = $(all_libraries) ./../lib/libfq.la
#
LogNotifier_SOURCES = LogNotifier.c
LogNotifier_CFLAGS = $(all_include_dirs)
LogNotifier_LDADD = $(all_libraries) ./../lib/libfq.la
#
enFQ_datafile_SOURCES = enFQ_datafile.c
enFQ_datafile_CFLAGS = $(all_include_dirs)
enFQ_datafile_LDADD = $(all_libraries) ./../lib/libfq.la
#
deQ_XA_enQ_SOURCES = deQ_XA_enQ.c
deQ_XA_enQ_CFLAGS = $(all_include_dirs)
deQ_XA_enQ_LDADD = $(all_libraries) ./../lib/libfq.la
#
lock_perfermance_TPS_SOURCES = lock_perfermance_TPS.c
lock_perfermance_TPS_CFLAGS = $(all_include_dirs)
lock_perfermance_TPS_LDADD = $(all_libraries) ./../lib/libfq.la
#
deFQ_bundle_SOURCES = deFQ_bundle.c
deFQ_bundle_CFLAGS = $(all_include_dirs)
deFQ_bundle_LDADD = $(all_libraries) ./../lib/libfq.la
#
mv_file_to_fq_SOURCES = mv_file_to_fq.c
mv_file_to_fq_CFLAGS = $(all_include_dirs)
mv_file_to_fq_LDADD = $(all_libraries) ./../lib/libfq.la
#
mv_file_b_to_fq_SOURCES = mv_file_b_to_fq.c
mv_file_b_to_fq_CFLAGS = $(all_include_dirs)
mv_file_b_to_fq_LDADD = $(all_libraries) ./../lib/libfq.la
#
mv_file_b_to_fq_multi_SOURCES = mv_file_b_to_fq_multi.c
mv_file_b_to_fq_multi_CFLAGS = $(all_include_dirs)
mv_file_b_to_fq_multi_LDADD = $(all_libraries) ./../lib/libfq.la
#
create_hash_map_SOURCES = create_hash_map.c
create_hash_map_CFLAGS = $(all_include_dirs)
create_hash_map_LDADD = $(all_libraries) ./../lib/libfq.la
#
unlink_hash_map_SOURCES = unlink_hash_map.c
unlink_hash_map_CFLAGS = $(all_include_dirs)
unlink_hash_map_LDADD = $(all_libraries) ./../lib/libfq.la
#
fq_snapshot_SOURCES = fq_snapshot.c
fq_snapshot_CFLAGS = $(all_include_dirs)
fq_snapshot_LDADD = $(all_libraries) ./../lib/libfq.la
#
fq_view_SOURCES = fq_view.c
fq_view_CFLAGS = $(all_include_dirs)
fq_view_LDADD = $(all_libraries) ./../lib/libfq.la
#
EXTRA_INST = L4.conf LogCollector.conf LogAgent.conf FQ_server.conf LA.sh LC.sh L4.sh TCP_sync_client.sh TCP_sync_server.conf FQ_Protocol_cli_txt_get.sh FQ_Protocol_cli_txt_put.sh FQ_Protocol_svr.conf EQ.sh DQ.sh log_files.list mv_file_to_fq.conf test.dat fqm_queue.list perftest_C.sh perftest_jni.sh fqm_queue.list
EXTRA_DIST = L4.conf LogCollector.conf LogAgent.conf FQ_server.conf LA.sh LC.sh L4.sh TCP_sync_client.sh TCP_sync_server.conf FQ_Protocol_cli_txt_get.sh FQ_Protocol_cli_txt_put.sh FQ_Protocol_svr.conf EQ.sh DQ.sh log_files.list mv_file_to_fq.conf snapshot.cfg test.dat fqm_queue.list perftest_C.sh perftest_jni.sh fqm_shm_queue.list

noinst_PROGRAMS =
