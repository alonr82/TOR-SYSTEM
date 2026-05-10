import { Scenario1Canvas } from "./scenarios/Scenario1Canvas";
import { Scenario2Canvas } from "./scenarios/Scenario2Canvas";
import { Scenario3Canvas } from "./scenarios/Scenario3Canvas";

export interface NetworkCanvasProps {
  currentScenario: 1 | 2 | 3;
  isAttacking: boolean;
  isDefending: boolean;
  addLog?: any; 
  backendEvent?: any; 
}

export function NetworkCanvas({
  currentScenario,
  isAttacking,
  isDefending,
  addLog,
  backendEvent
}: NetworkCanvasProps) {

  return (
    <div className="w-full h-full relative overflow-hidden">
      {currentScenario === 1 && (
        <Scenario1Canvas
          isAttacking={isAttacking}
          isDefending={isDefending}
          backendEvent={backendEvent}
          addLog={addLog} 
        />
      )}
      {currentScenario === 2 && (
        <Scenario2Canvas
          isAttacking={isAttacking}
          isDefending={isDefending}
          backendEvent={backendEvent}
          addLog={addLog} 
        />
      )}
      {currentScenario === 3 && (
        <Scenario3Canvas
          isAttacking={isAttacking}
          isDefending={isDefending}
          backendEvent={backendEvent}
          addLog={addLog} 
        />
      )}
    </div>
  );
}