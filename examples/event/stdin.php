<?php
swoole_event_add(STDIN, function($fp) {
	echo "STDIN: ".fread($fp, 8192);
} ,  null, SWOOLE_EVENT_READ);
