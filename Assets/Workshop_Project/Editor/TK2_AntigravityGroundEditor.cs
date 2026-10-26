using System;
using System.IO;
using UnityEditor;
using UnityEngine;

namespace TK2.Antigravity
{
    // Standard Transform metadata survives bundles without shipping a new script type.
    internal static class Surface
    {
        internal const string Marker = "__TK2_Antigravity_v1";

        internal static bool Enabled(PTK_GroundType ground) => ground.transform.Find(Marker) != null;

        internal static void Set(PTK_GroundType ground, bool enabled)
        {
            var marker = ground.transform.Find(Marker);
            if (enabled && marker == null)
            {
                var child = new GameObject(Marker);
                Undo.RegisterCreatedObjectUndo(child, "Enable antigravity surface");
                child.transform.SetParent(ground.transform, false);
            }
            else if (!enabled && marker != null) Undo.DestroyObjectImmediate(marker.gameObject);
            EditorUtility.SetDirty(ground.gameObject);
        }
    }

    [CustomEditor(typeof(PTK_GroundType)), CanEditMultipleObjects]
    public sealed class GroundInspector : UnityEditor.Editor
    {
        public override void OnInspectorGUI()
        {
            DrawDefaultInspector();
            EditorGUILayout.Space();
            EditorGUILayout.LabelField("Antigravity", EditorStyles.boldLabel);
            bool enabled = Surface.Enabled((PTK_GroundType)target);
            bool mixed = false;
            foreach (PTK_GroundType ground in targets) mixed |= Surface.Enabled(ground) != enabled;
            EditorGUI.showMixedValue = mixed;
            EditorGUI.BeginChangeCheck();
            bool selected = EditorGUILayout.Toggle(new GUIContent("Antigravity surface",
                "Opt this collider into road-relative gravity. Ordinary ground settings still apply."), enabled);
            if (EditorGUI.EndChangeCheck())
            {
                int group = Undo.GetCurrentGroup();
                foreach (PTK_GroundType ground in targets)
                {
                    if (selected && ground.GetComponent<Collider>() == null)
                    { Debug.LogError("Antigravity ground needs a Collider on the same object.", ground); continue; }
                    Surface.Set(ground, selected);
                }
                Undo.CollapseUndoOperations(group);
            }
            EditorGUI.showMixedValue = false;
            if (enabled || mixed)
                EditorGUILayout.HelpBox("Requires the Antigravity game runtime. Without it, this remains ordinary ground. " +
                    "Mark only intended loop/ceiling sections; keep a continuous physical road and AI path.", MessageType.Info);
        }
    }

    public static class EditorRelease
    {
        private const string Folder = "Assets/Workshop_Project/Antigravity";
        private const string Example = Folder + "/Example Ground.prefab";

        // Batch entry point: verifies a real AssetBundle round trip before exporting.
        public static void Build()
        {
            Directory.CreateDirectory(Folder);
            Directory.CreateDirectory("local/sdk-export-check");
            Directory.CreateDirectory("Releases");
            GameObject example = GameObject.CreatePrimitive(PrimitiveType.Cube);
            try
            {
                example.name = "Antigravity Ground Example";
                example.transform.localScale = new Vector3(12f, .2f, 12f);
                var ground = example.AddComponent<PTK_GroundType>();
                Surface.Set(ground, true);
                if (!Surface.Enabled(ground)) throw new InvalidOperationException("Marker creation failed.");
                Surface.Set(ground, false);
                if (Surface.Enabled(ground)) throw new InvalidOperationException("Marker removal failed.");
                Surface.Set(ground, true);
                PrefabUtility.SaveAsPrefabAsset(example, Example);
            }
            finally { UnityEngine.Object.DestroyImmediate(example); }
            AssetDatabase.SaveAssets();
            var builds = new[] { new AssetBundleBuild { assetBundleName = "antigravity-check", assetNames = new[] { Example } } };
            var manifest = BuildPipeline.BuildAssetBundles("local/sdk-export-check", builds,
                BuildAssetBundleOptions.ForceRebuildAssetBundle, BuildTarget.StandaloneWindows64);
            if (manifest == null) throw new InvalidOperationException("AssetBundle build failed.");
            var bundle = AssetBundle.LoadFromFile("local/sdk-export-check/antigravity-check");
            if (bundle == null) throw new InvalidOperationException("AssetBundle reload failed.");
            try
            {
                var prefab = bundle.LoadAsset<GameObject>(Example);
                if (prefab == null || prefab.GetComponent<PTK_GroundType>() == null ||
                    prefab.GetComponent<Collider>() == null || prefab.transform.Find(Surface.Marker) == null)
                    throw new InvalidOperationException("Exported ground/Collider/marker did not survive the round trip.");
                if (prefab.GetComponentsInChildren<MonoBehaviour>(true).Length != 1)
                    throw new InvalidOperationException("The example contains an unexpected runtime script.");
            }
            finally { bundle.Unload(true); }
            AssetDatabase.ExportPackage(new[] { "Assets/Workshop_Project/Editor/TK2_AntigravityGroundEditor.cs", Folder },
                "Releases/TK2-Antigravity-SDK-0.1.1.unitypackage", ExportPackageOptions.Recurse);
            Debug.Log("TK2 ANTIGRAVITY SDK: marker round trip verified; release package exported.");
        }
    }
}
