
//Task1code
void Task1code( void * pvParameters )
{ 
  vTaskDelay(500);
  DebugPrintln(3, "----------------------------------------------------------------");
  DebugPrintln(3, "---------------------------- TaskCode1 -------------------------");
  DebugPrintln(3, " DETAILS :");
  DebugPrint(3, " TaskCode1 - Task1       - Running On CORE ");
  DebugPrintln(3, xPortGetCoreID());
  DebugPrintln(3, " TaskCode1 - Task1       - COMPLETED");
  DebugPrintln(3, "----------------------------------------------------------------");
  for(;;)
  {
    vTaskDelay(200);
    //vTaskDelete(NULL);
  }
}