import { LoginScreen } from "@/components/auth/LoginScreen";
import { AppShell } from "@/components/mail/AppShell";
import { useMailbox } from "@/hooks/useMailbox";
import { runMailUtilsTests } from "@/utils/mail";

if (typeof window !== "undefined" && import.meta.env.DEV) {
  console.assert(runMailUtilsTests(), "Utility tests failed");
}

export default function App() {
  const mailbox = useMailbox();

  if (!mailbox || !mailbox.state || !mailbox.actions) {
    return <div>Loading...</div>;
  }

  const { state, actions } = mailbox;

  if (!state.isAuthenticated) {
    return <LoginScreen onLogin={actions.login} />;
  }

  return <AppShell state={state} actions={actions} />;
}