namespace GmmlPatcher;

public interface IGameMakerMod {
    // audioGroup 0 = game data
    public void Load(int audioGroup, ModData currentMod);
    public void LateLoad(int audioGroup, ModData currentMod) { }
}
