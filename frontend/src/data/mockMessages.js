export const SEND_STAGES = ["Encrypting...", "Routing...", "Sent Securely"];
export const DATE_FILTERS = ["Any time", "Today", "Last 7 days", "Last 30 days"];
export const FOLDERS = ["Inbox", "Sent", "Drafts", "Trash"];

export const initialMessages = [
  {
    id: "m-1001",
    folder: "Inbox",
    fromName: "Support Team",
    fromId: "support@sams.io",
    to: "alex@sams.io",
    subject: "Welcome to SAMS Secure Mail",
    preview:
      "Your secure inbox is ready. Search, filter, and route messages privately without exposing transport details.",
    body: [
      "Welcome to SAMS.",
      "Your account is now active and protected through layered anonymous delivery. Messages are presented in a familiar mail workflow while private routing, encryption, and backend session setup remain fully abstracted from the user experience.",
      "From this workspace you can search and filter your secure inbox, review attachments, compose new messages, and monitor simple state feedback such as Encrypting, Routing, and Sent Securely.",
      "Regards,",
      "SAMS Support",
    ],
    timestamp: "2026-03-17T10:45:00",
    dateLabel: "10:45 AM",
    unread: true,
    verified: true,
    encrypted: true,
    attachments: [],
  },
  {
    id: "m-1002",
    folder: "Inbox",
    fromName: "anon-user-823",
    fromId: "anon-823@sams.internal",
    to: "alex@sams.io",
    subject: "Project Delta: Security Protocols",
    preview:
      "Please review the encrypted documentation package and confirm your local environment is ready for the next release cycle.",
    body: [
      "Hello,",
      "Please find the attached documentation regarding the latest security updates for Project Delta. All protocols must be followed as outlined in the encrypted document to ensure compliance with the current rollout phase.",
      "If you encounter any issues accessing the file, verify your local credentials and session integrity. The protected delivery itself has already been completed successfully.",
      "Best regards,",
      "Anon User 823",
    ],
    timestamp: "2026-03-17T09:42:00",
    dateLabel: "09:42 AM",
    unread: false,
    verified: true,
    encrypted: true,
    attachments: [
      { id: "a-1", name: "encrypted_doc.pdf", size: "2.4 MB", kind: "PDF" },
    ],
  },
  {
    id: "m-1003",
    folder: "Inbox",
    fromName: "Sarah Connor",
    fromId: "sconnor@sams.partner",
    to: "alex@sams.io",
    subject: "Security Audit Results",
    preview:
      "The latest scan completed successfully. Review the summary and keep the signed attachment for record retention.",
    body: [
      "Hi Alex,",
      "The latest audit scan has been completed without major findings. I attached the signed summary for your archive.",
      "The transport layer remained fully private throughout delivery, and message integrity was verified on receipt.",
      "Thanks,",
      "Sarah",
    ],
    timestamp: "2026-03-16T18:20:00",
    dateLabel: "Yesterday",
    unread: false,
    verified: true,
    encrypted: true,
    attachments: [
      { id: "a-2", name: "audit_summary.txt", size: "180 KB", kind: "TXT" },
    ],
  },
  {
    id: "m-1004",
    folder: "Inbox",
    fromName: "Internal Comms",
    fromId: "broadcast@sams.internal",
    to: "alex@sams.io",
    subject: "Updated retention policy",
    preview:
      "Please read the revised mailbox retention guidance. Search filters now support sender and date refinements.",
    body: [
      "Team,",
      "Mailbox retention guidance has been updated. The inbox now emphasizes familiar productivity behaviors while preserving secure delivery under the surface.",
      "You can filter by sender and date directly from the secure inbox toolbar.",
      "Internal Communications",
    ],
    timestamp: "2026-03-15T11:30:00",
    dateLabel: "Mar 15",
    unread: false,
    verified: true,
    encrypted: true,
    attachments: [],
  },
  {
    id: "m-1005",
    folder: "Sent",
    fromName: "Alex Rivera",
    fromId: "alex@sams.io",
    to: "ops@sams.internal",
    subject: "Release checklist attached",
    preview:
      "Sending the latest release checklist through secure delivery. Please confirm receipt once reviewed.",
    body: [
      "Ops team,",
      "Attached is the latest release checklist. Delivery was initiated from the secure compose workflow.",
      "Regards,",
      "Alex",
    ],
    timestamp: "2026-03-14T14:02:00",
    dateLabel: "Mar 14",
    unread: false,
    verified: true,
    encrypted: true,
    attachments: [
      { id: "a-3", name: "release_checklist.pdf", size: "940 KB", kind: "PDF" },
    ],
  },
];

export const liveMessage = {
  id: "m-live-2001",
  folder: "Inbox",
  fromName: "Realtime Ops",
  fromId: "ops-live@sams.internal",
  to: "alex@sams.io",
  subject: "Live route update available",
  preview:
    "A new protected message reached your inbox through the live delivery channel. No route details are exposed.",
  body: [
    "Hello Alex,",
    "This message was delivered through the real-time synchronization layer. The interface intentionally shows only delivery confidence and inbox state, not backend transport details.",
    "You can review it like any other message in the secure inbox.",
    "Realtime Ops",
  ],
  timestamp: "2026-03-17T11:18:00",
  dateLabel: "11:18 AM",
  unread: true,
  verified: true,
  encrypted: true,
  attachments: [
    { id: "a-live", name: "delivery_note.txt", size: "32 KB", kind: "TXT" },
  ],
};