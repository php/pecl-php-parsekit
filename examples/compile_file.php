<?php
  /* Compile ourself */

  $oparray = parsekit_compile_file($_SERVER['PHP_SELF']);

  var_dump($oparray);
