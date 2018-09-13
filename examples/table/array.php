<?php
$table = new swoole_table(1024);
$table->column('id', swoole_table::TYPE_INT);
$table->column('name', swoole_table::TYPE_STRING, 64);
$table->column('num', swoole_table::TYPE_FLOAT);
$table->create();

$table['apple'] = array('id' => 145, 'name' => 'iPhone', 'num' => 3.1415);
 
 var_dump($table['apple']);
 //$table->set('apple',array('id' => 145, 'name' => 'iPhone', 'num' => 3.1415));


// $table['google'] = array('id' => 358, 'name' => "AlphaGo", 'num' => 3.1415);

// $table['microsoft']['name'] = "Windows";
// $table['microsoft']['num'] = '1997.03';

// var_dump($table['apple']);
// var_dump($table['microsoft']);

// $table['google']['num'] = 500.90;
// var_dump($table['google']);


class obj implements arrayaccess {
    private $container = array();
    public function __construct() {
        $this->container = array(
            "one"   => 1,
            "two"   => 2,
            "three" => 3,
        );
    }
    public function offsetSet($offset, $value) {
        if (is_null($offset)) {
            $this->container[] = $value;
        } else {
            $this->container[$offset] = $value;
        }
    }
    public function offsetExists($offset) {
        return isset($this->container[$offset]);
    }
    public function offsetUnset($offset) {
        unset($this->container[$offset]);
    }
    public function offsetGet($offset) {
        return isset($this->container[$offset]) ? $this->container[$offset] : null;
    }
}

$obj = new obj;

// var_dump(isset($obj["two"]));
// var_dump($obj["two"]);
// unset($obj["two"]);
// var_dump(isset($obj["two"]));
// $obj["two"] = "A value";
// var_dump($obj["two"]);
// $obj[] = 'Append 1';
// $obj[] = 'Append 2';
// $obj[] = 'Append 3';
// print_r($obj);

//$a['appy'] = "test";


