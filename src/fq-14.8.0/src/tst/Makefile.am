all_include_dirs = -g\
	-I$(top_srcdir)/src/lib
bin_PROGRAMS = \
		conf_test\
		deFQ_tst\
		enFQ_tst\
		scanf_test\
		tci_test\
		tokenizer\
		var_multi_open\
		var_array_multi_open\
		GetSequence\
		sequence_obj_test\
		heartbeat_test\
		heartbeat_change\
		trace_test\
		env_test\
		deFQ_heartbeat\
		heartbeat_controller\
		delimeter_parser\
		codemap_test\
		sec_count\
		flock_test\
		map_db_test\
		find_pid_fds\
		syslog_test\
		StopWatch\
		sema_queue_test\
		hashmap_test\
		hashobj_test\
		mask_test\
		lsof_test\
		file_table_test\
		multi_conf_test\
		make_threads_from_filelist\
		stopwatch\
		TCP_sync_server\
		TCP_sync_client\
		codemap_tokenizer_test\
		ConvMessage\
		fq_container\
		gssi_test\
		delimiter_list\
		cache_test\
		deQ_server_sample\
		enQ_server_sample\
		pthread_cancel\
		replace_test\
		hash_config\
		move_queue\
		filelist_matrix\
		milisecond\
		qsort\
		uhash_test\
		json_test\
		json_append_array_test\
		on_view_tst\
		on_do_not_flow\
		on_check_competition\
		get_fuser\
		TCP_sync_test\
		GenTestDataFile\
		enFQ_bundle\
		deFQ_XA_dump\
		enFQ_from_dumpfile\
		md5_test\
		json_gssi_gen_test\
		bundle_move\
		enFQ_circular\
		base64_decode\
		parson_write_sample\
		parson_read_sample\
		conv_msg_2_json\
		json_config_test\
		MakeListFromDelimiterMsg\
		write_log_flock\
		read_log_flock\
		safe_write_test\
		safe_read_test\
		safe_write_b_test\
		safe_read_multi_test\
		fq_callback_test\
		fq_dir_CB_test\
		ListCallback_test\
		shmq_callback_test\
		fq_container_CB_test\
		tci_aformat_json\
		linkedlist_test\
		tci_aformat_json\
		fq_process_manage_test\
		all_queue_info\
		is_working_time\
		ratio_distribute_tst\
		all_queue_scan_server\
		qobjs\
		fq_btree_tst\
		fq_btree_obj_tst\
		json_rule_make_tst
# 
if HAVE_SOCKET
all_libraries = -lm -ldl -lnsl -lrt -lpthread -lsocket -lm
else
all_libraries = -lm -ldl -lrt -lpthread -lm
endif

#
conf_test_SOURCES = conf_test.c
conf_test_CFLAGS = $(all_include_dirs)
conf_test_LDADD = $(all_libraries) ./../lib/libfq.la
#
codemap_test_SOURCES = codemap_test.c
codemap_test_CFLAGS = $(all_include_dirs)
codemap_test_LDADD = $(all_libraries) ./../lib/libfq.la
#
deFQ_tst_SOURCES = deFQ_tst.c
deFQ_tst_CFLAGS = $(all_include_dirs)
deFQ_tst_LDADD = $(all_libraries) ./../lib/libfq.la
#
enFQ_tst_SOURCES = enFQ_tst.c
enFQ_tst_CFLAGS = $(all_include_dirs)
enFQ_tst_LDADD = $(all_libraries) ./../lib/libfq.la
#
scanf_test_SOURCES = scanf_test.c
scanf_test_CFLAGS = $(all_include_dirs)
scanf_test_LDADD = $(all_libraries) ./../lib/libfq.la
#
tci_test_SOURCES = tci_test.c
tci_test_CFLAGS = $(all_include_dirs)
tci_test_LDADD = $(all_libraries) ./../lib/libfq.la
#
tokenizer_SOURCES = tokenizer.c
tokenizer_CFLAGS = $(all_include_dirs)
tokenizer_LDADD = $(all_libraries) ./../lib/libfq.la
#
var_multi_open_SOURCES = var_multi_open.c
var_multi_open_CFLAGS = $(all_include_dirs)
var_multi_open_LDADD = $(all_libraries) ./../lib/libfq.la
#
var_array_multi_open_SOURCES = var_array_multi_open.c
var_array_multi_open_CFLAGS = $(all_include_dirs)
var_array_multi_open_LDADD = $(all_libraries) ./../lib/libfq.la
#
GetSequence_SOURCES = GetSequence.c
GetSequence_CFLAGS = $(all_include_dirs)
GetSequence_LDADD = $(all_libraries) ./../lib/libfq.la
#
sequence_obj_test_SOURCES = sequence_obj_test.c
sequence_obj_test_CFLAGS = $(all_include_dirs)
sequence_obj_test_LDADD = $(all_libraries) ./../lib/libfq.la
#
heartbeat_test_SOURCES = heartbeat_test.c
heartbeat_test_CFLAGS = $(all_include_dirs)
heartbeat_test_LDADD = $(all_libraries) ./../lib/libfq.la
#
heartbeat_change_SOURCES = heartbeat_change.c
heartbeat_change_CFLAGS = $(all_include_dirs)
heartbeat_change_LDADD = $(all_libraries) ./../lib/libfq.la
#
trace_test_SOURCES = trace_test.c
trace_test_CFLAGS = $(all_include_dirs)
trace_test_LDADD = $(all_libraries) ./../lib/libfq.la
#
env_test_SOURCES = env_test.c
env_test_CFLAGS = $(all_include_dirs)
env_test_LDADD = $(all_libraries) ./../lib/libfq.la
#
deFQ_heartbeat_SOURCES = deFQ_heartbeat.c
deFQ_heartbeat_CFLAGS = $(all_include_dirs)
deFQ_heartbeat_LDADD = $(all_libraries) ./../lib/libfq.la
#
heartbeat_controller_SOURCES = heartbeat_controller.c
heartbeat_controller_CFLAGS = $(all_include_dirs)
heartbeat_controller_LDADD = $(all_libraries) ./../lib/libfq.la
#
delimeter_parser_SOURCES = delimeter_parser.c
delimeter_parser_CFLAGS = $(all_include_dirs)
delimeter_parser_LDADD = $(all_libraries) ./../lib/libfq.la
#
sec_count_SOURCES = sec_count.c
sec_count_CFLAGS = $(all_include_dirs)
sec_count_LDADD = $(all_libraries) ./../lib/libfq.la
#
flock_test_SOURCES = flock_test.c
flock_test_CFLAGS = $(all_include_dirs)
flock_test_LDADD = $(all_libraries) ./../lib/libfq.la
#
map_db_test_SOURCES = map_db_test.c
map_db_test_CFLAGS = $(all_include_dirs)
map_db_test_LDADD = $(all_libraries) ./../lib/libfq.la
#
find_pid_fds_SOURCES = find_pid_fds.c
find_pid_fds_CFLAGS = $(all_include_dirs)
find_pid_fds_LDADD = $(all_libraries) ./../lib/libfq.la
#
syslog_test_SOURCES = syslog_test.c
syslog_test_CFLAGS = $(all_include_dirs)
syslog_test_LDADD = $(all_libraries) ./../lib/libfq.la
#
StopWatch_SOURCES = StopWatch.c
StopWatch_CFLAGS = $(all_include_dirs)
StopWatch_LDADD = $(all_libraries) ./../lib/libfq.la
#
sema_queue_test_SOURCES = sema_queue_test.c
sema_queue_test_CFLAGS = $(all_include_dirs)
sema_queue_test_LDADD = $(all_libraries) ./../lib/libfq.la
#
hashmap_test_SOURCES = hashmap_test.c
hashmap_test_CFLAGS = $(all_include_dirs)
hashmap_test_LDADD = $(all_libraries) ./../lib/libfq.la
#
hashobj_test_SOURCES = hashobj_test.c
hashobj_test_CFLAGS = $(all_include_dirs)
hashobj_test_LDADD = $(all_libraries) ./../lib/libfq.la
#
mask_test_SOURCES = mask_test.c
mask_test_CFLAGS = $(all_include_dirs)
mask_test_LDADD = $(all_libraries) ./../lib/libfq.la
#
lsof_test_SOURCES = lsof_test.c
lsof_test_CFLAGS = $(all_include_dirs)
lsof_test_LDADD = $(all_libraries) ./../lib/libfq.la
#
file_table_test_SOURCES = file_table_test.c
file_table_test_CFLAGS = $(all_include_dirs)
file_table_test_LDADD = $(all_libraries) ./../lib/libfq.la
#
multi_conf_test_SOURCES = multi_conf_test.c
multi_conf_test_CFLAGS = $(all_include_dirs)
multi_conf_test_LDADD = $(all_libraries) ./../lib/libfq.la
#
make_threads_from_filelist_SOURCES = make_threads_from_filelist.c
make_threads_from_filelist_CFLAGS = $(all_include_dirs)
make_threads_from_filelist_LDADD = $(all_libraries) ./../lib/libfq.la
#
stopwatch_SOURCES = stopwatch.c
stopwatch_CFLAGS = $(all_include_dirs)
stopwatch_LDADD = $(all_libraries) ./../lib/libfq.la
#
TCP_sync_server_SOURCES = TCP_sync_server.c
TCP_sync_server_CFLAGS = $(all_include_dirs)
TCP_sync_server_LDADD = $(all_libraries) ./../lib/libfq.la
#
TCP_sync_client_SOURCES = TCP_sync_client.c
TCP_sync_client_CFLAGS = $(all_include_dirs)
TCP_sync_client_LDADD = $(all_libraries) ./../lib/libfq.la
#
codemap_tokenizer_test_SOURCES = codemap_tokenizer_test.c
codemap_tokenizer_test_CFLAGS = $(all_include_dirs)
codemap_tokenizer_test_LDADD = $(all_libraries) ./../lib/libfq.la
#
ConvMessage_SOURCES = ConvMessage.c
ConvMessage_CFLAGS = $(all_include_dirs)
ConvMessage_LDADD = $(all_libraries) ./../lib/libfq.la
#
fq_container_SOURCES = fq_container.c
fq_container_CFLAGS = $(all_include_dirs)
fq_container_LDADD = $(all_libraries) ./../lib/libfq.la
#
gssi_test_SOURCES = gssi_test.c
gssi_test_CFLAGS = $(all_include_dirs)
gssi_test_LDADD = $(all_libraries) ./../lib/libfq.la
#
delimiter_list_SOURCES = delimiter_list.c
delimiter_list_CFLAGS = $(all_include_dirs)
delimiter_list_LDADD = $(all_libraries) ./../lib/libfq.la
#
cache_test_SOURCES = cache_test.c
cache_test_CFLAGS = $(all_include_dirs)
cache_test_LDADD = $(all_libraries) ./../lib/libfq.la
#
deQ_server_sample_SOURCES = deQ_server_sample.c
deQ_server_sample_CFLAGS = $(all_include_dirs)
deQ_server_sample_LDADD = $(all_libraries) ./../lib/libfq.la
#
enQ_server_sample_SOURCES = enQ_server_sample.c
enQ_server_sample_CFLAGS = $(all_include_dirs)
enQ_server_sample_LDADD = $(all_libraries) ./../lib/libfq.la
#
pthread_cancel_SOURCES = pthread_cancel.c
pthread_cancel_CFLAGS = $(all_include_dirs)
pthread_cancel_LDADD = $(all_libraries) ./../lib/libfq.la
#
replace_test_SOURCES = replace_test.c
replace_test_CFLAGS = $(all_include_dirs)
replace_test_LDADD = $(all_libraries) ./../lib/libfq.la
#
hash_config_SOURCES = hash_config.c
hash_config_CFLAGS = $(all_include_dirs)
hash_config_LDADD = $(all_libraries) ./../lib/libfq.la
#
move_queue_SOURCES = move_queue.c
move_queue_CFLAGS = $(all_include_dirs)
move_queue_LDADD = $(all_libraries) ./../lib/libfq.la
#
filelist_matrix_SOURCES = filelist_matrix.c
filelist_matrix_CFLAGS = $(all_include_dirs)
filelist_matrix_LDADD = $(all_libraries) ./../lib/libfq.la
#
milisecond_SOURCES = milisecond.c
milisecond_CFLAGS = $(all_include_dirs)
milisecond_LDADD = $(all_libraries) ./../lib/libfq.la
#
qsort_SOURCES = qsort.c
qsort_CFLAGS = $(all_include_dirs)
qsort_LDADD = $(all_libraries) ./../lib/libfq.la
#
uhash_test_SOURCES = uhash_test.c
uhash_test_CFLAGS = $(all_include_dirs)
uhash_test_LDADD = $(all_libraries) ./../lib/libfq.la
#
json_test_SOURCES = json_test.c
json_test_CFLAGS = $(all_include_dirs)
json_test_LDADD = $(all_libraries) ./../lib/libfq.la
#
json_append_array_test_SOURCES = json_append_array_test.c
json_append_array_test_CFLAGS = $(all_include_dirs)
json_append_array_test_LDADD = $(all_libraries) ./../lib/libfq.la
#
on_view_tst_SOURCES = on_view_tst.c
on_view_tst_CFLAGS = $(all_include_dirs)
on_view_tst_LDADD = $(all_libraries) ./../lib/libfq.la
#
on_do_not_flow_SOURCES = on_do_not_flow.c
on_do_not_flow_CFLAGS = $(all_include_dirs)
on_do_not_flow_LDADD = $(all_libraries) ./../lib/libfq.la
#
on_check_competition_SOURCES = on_check_competition.c
on_check_competition_CFLAGS = $(all_include_dirs)
on_check_competition_LDADD = $(all_libraries) ./../lib/libfq.la
#
get_fuser_SOURCES = get_fuser.c
get_fuser_CFLAGS = $(all_include_dirs)
get_fuser_LDADD = $(all_libraries) ./../lib/libfq.la
#
TCP_sync_test_SOURCES = TCP_sync_test.c
TCP_sync_test_CFLAGS = $(all_include_dirs)
TCP_sync_test_LDADD = $(all_libraries) ./../lib/libfq.la
#
GenTestDataFile_SOURCES = GenTestDataFile.c
GenTestDataFile_CFLAGS = $(all_include_dirs)
GenTestDataFile_LDADD = $(all_libraries) ./../lib/libfq.la
#
enFQ_bundle_SOURCES = enFQ_bundle.c
enFQ_bundle_CFLAGS = $(all_include_dirs)
enFQ_bundle_LDADD = $(all_libraries) ./../lib/libfq.la
#
deFQ_XA_dump_SOURCES = deFQ_XA_dump.c
deFQ_XA_dump_CFLAGS = $(all_include_dirs)
deFQ_XA_dump_LDADD = $(all_libraries) ./../lib/libfq.la
#
enFQ_from_dumpfile_SOURCES = enFQ_from_dumpfile.c
enFQ_from_dumpfile_CFLAGS = $(all_include_dirs)
enFQ_from_dumpfile_LDADD = $(all_libraries) ./../lib/libfq.la
#
md5_test_SOURCES = md5_test.c
md5_test_CFLAGS = $(all_include_dirs)
md5_test_LDADD = $(all_libraries) ./../lib/libfq.la
#
json_gssi_gen_test_SOURCES = json_gssi_gen_test.c
json_gssi_gen_test_CFLAGS = $(all_include_dirs)
json_gssi_gen_test_LDADD = $(all_libraries) ./../lib/libfq.la
#
bundle_move_SOURCES = bundle_move.c
bundle_move_CFLAGS = $(all_include_dirs)
bundle_move_LDADD = $(all_libraries) ./../lib/libfq.la
#
enFQ_circular_SOURCES = enFQ_circular.c
enFQ_circular_CFLAGS = $(all_include_dirs)
enFQ_circular_LDADD = $(all_libraries) ./../lib/libfq.la
#
base64_decode_SOURCES = base64_decode.c
base64_decode_CFLAGS = $(all_include_dirs)
base64_decode_LDADD = $(all_libraries) ./../lib/libfq.la
#
parson_write_sample_SOURCES = parson_write_sample.c
parson_write_sample_CFLAGS = $(all_include_dirs)
parson_write_sample_LDADD = $(all_libraries) ./../lib/libfq.la
#
parson_read_sample_SOURCES = parson_read_sample.c
parson_read_sample_CFLAGS = $(all_include_dirs)
parson_read_sample_LDADD = $(all_libraries) ./../lib/libfq.la
#
conv_msg_2_json_SOURCES = conv_msg_2_json.c
conv_msg_2_json_CFLAGS = $(all_include_dirs)
conv_msg_2_json_LDADD = $(all_libraries) ./../lib/libfq.la
#
json_config_test_SOURCES = json_config_test.c
json_config_test_CFLAGS = $(all_include_dirs)
json_config_test_LDADD = $(all_libraries) ./../lib/libfq.la
#
MakeListFromDelimiterMsg_SOURCES = MakeListFromDelimiterMsg.c
MakeListFromDelimiterMsg_CFLAGS = $(all_include_dirs)
MakeListFromDelimiterMsg_LDADD = $(all_libraries) ./../lib/libfq.la
#
write_log_flock_SOURCES = write_log_flock.c
write_log_flock_CFLAGS = $(all_include_dirs)
write_log_flock_LDADD = $(all_libraries) ./../lib/libfq.la
#
read_log_flock_SOURCES = read_log_flock.c
read_log_flock_CFLAGS = $(all_include_dirs)
read_log_flock_LDADD = $(all_libraries) ./../lib/libfq.la
#
safe_write_test_SOURCES = safe_write_test.c
safe_write_test_CFLAGS = $(all_include_dirs)
safe_write_test_LDADD = $(all_libraries) ./../lib/libfq.la
#
safe_read_test_SOURCES = safe_read_test.c
safe_read_test_CFLAGS = $(all_include_dirs)
safe_read_test_LDADD = $(all_libraries) ./../lib/libfq.la
#
safe_write_b_test_SOURCES = safe_write_b_test.c
safe_write_b_test_CFLAGS = $(all_include_dirs)
safe_write_b_test_LDADD = $(all_libraries) ./../lib/libfq.la
#
safe_read_multi_test_SOURCES = safe_read_multi_test.c
safe_read_multi_test_CFLAGS = $(all_include_dirs)
safe_read_multi_test_LDADD = $(all_libraries) ./../lib/libfq.la
#
fq_callback_test_SOURCES = fq_callback_test.c
fq_callback_test_CFLAGS = $(all_include_dirs)
fq_callback_test_LDADD = $(all_libraries) ./../lib/libfq.la
#
fq_dir_CB_test_SOURCES = fq_dir_CB_test.c
fq_dir_CB_test_CFLAGS = $(all_include_dirs)
fq_dir_CB_test_LDADD = $(all_libraries) ./../lib/libfq.la
#
ListCallback_test_SOURCES = ListCallback_test.c
ListCallback_test_CFLAGS = $(all_include_dirs)
ListCallback_test_LDADD = $(all_libraries) ./../lib/libfq.la
#
shmq_callback_test_SOURCES = shmq_callback_test.c
shmq_callback_test_CFLAGS = $(all_include_dirs)
shmq_callback_test_LDADD = $(all_libraries) ./../lib/libfq.la
#
fq_container_CB_test_SOURCES = fq_container_CB_test.c
fq_container_CB_test_CFLAGS = $(all_include_dirs)
fq_container_CB_test_LDADD = $(all_libraries) ./../lib/libfq.la
#
tci_aformat_json_SOURCES = tci_aformat_json.c
tci_aformat_json_CFLAGS = $(all_include_dirs)
tci_aformat_json_LDADD = $(all_libraries) ./../lib/libfq.la
#
linkedlist_test_SOURCES = linkedlist_test.c
linkedlist_test_CFLAGS = $(all_include_dirs)
linkedlist_test_LDADD = $(all_libraries) ./../lib/libfq.la
#
fq_process_manage_test_SOURCES = fq_process_manage_test.c
fq_process_manage_test_CFLAGS = $(all_include_dirs)
fq_process_manage_test_LDADD = $(all_libraries) ./../lib/libfq.la
#
all_queue_info_SOURCES = all_queue_info.c
all_queue_info_CFLAGS = $(all_include_dirs)
all_queue_info_LDADD = $(all_libraries) ./../lib/libfq.la
#
is_working_time_SOURCES = is_working_time.c
is_working_time_CFLAGS = $(all_include_dirs)
is_working_time_LDADD = $(all_libraries) ./../lib/libfq.la
#
ratio_distribute_tst_SOURCES = ratio_distribute_tst.c
ratio_distribute_tst_CFLAGS = $(all_include_dirs)
ratio_distribute_tst_LDADD = $(all_libraries) ./../lib/libfq.la
#
all_queue_scan_server_SOURCES = all_queue_scan_server.c
all_queue_scan_server_CFLAGS = $(all_include_dirs)
all_queue_scan_server_LDADD = $(all_libraries) ./../lib/libfq.la
#
qobjs_SOURCES = qobjs.c
qobjs_CFLAGS = $(all_include_dirs)
qobjs_LDADD = $(all_libraries) ./../lib/libfq.la
#
fq_btree_tst_SOURCES = fq_btree_tst.c
fq_btree_tst_CFLAGS = $(all_include_dirs)
fq_btree_tst_LDADD = $(all_libraries) ./../lib/libfq.la
#
fq_btree_obj_tst_SOURCES = fq_btree_obj_tst.c
fq_btree_obj_tst_CFLAGS = $(all_include_dirs)
fq_btree_obj_tst_LDADD = $(all_libraries) ./../lib/libfq.la
#
json_rule_make_tst_SOURCES = json_rule_make_tst.c
json_rule_make_tst_CFLAGS = $(all_include_dirs)
json_rule_make_tst_LDADD = $(all_libraries) ./../lib/libfq.la
#
EXTRA_DIST =  aformat.sample qformat.sample tst.cfg test.txt bankcode.map crontab multi_conf_sample.conf TCP_sync_server.conf CRM.MSG example.json gssi_templete.json config.json TST.Q.info ListFQ.info FQ_directories.list test.Hash.info read_log_flock.conf FQ_external_env.conf co_queue.map json_make.rule sample_in.json
noinst_PROGRAMS = 
