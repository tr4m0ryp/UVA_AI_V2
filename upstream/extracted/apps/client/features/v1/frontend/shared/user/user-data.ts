export async function DeleteAccountForCurrentUser(_options?: Record<string, unknown>): Promise<void> {}
export async function GetUserSettings(): Promise<Array<{ settingKey: string; settingValue: unknown }>> { return [] }
export async function SaveUserSetting(_key: string, _value: unknown): Promise<void> {}
