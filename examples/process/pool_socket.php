<?php
$pool = new Swoole\Process\Pool(1, SWOOLE_IPC_SOCKET);

$pool->on("Message", function ($pool, $message) {
    echo "Message: {$message}\n";
    $pool->write("hello ");
    $pool->write("world ");
    $pool->write("\n");
});

$pool->listen('127.0.0.1', 8089);

$pool->start();
