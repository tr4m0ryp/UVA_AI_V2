import { Plugin } from "@opencode-ai/sdk"

export default Plugin(async (input) => {
    return {
        /* Identify the client and project directory to the proxy */
        "chat.headers": async () => ({
            "X-Opencode-Client": "opencode",
            "X-Project-Dir": input.directory,
        }),

        /* Enrich the system prompt with project working directory */
        "experimental.chat.system.transform": async (parts: any[], _opts: any) => {
            return [
                ...parts,
                {
                    type: "text" as const,
                    text: `Working directory: ${input.directory}\nProject: ${input.project?.name ?? "unknown"}`,
                },
            ]
        },
    }
})
