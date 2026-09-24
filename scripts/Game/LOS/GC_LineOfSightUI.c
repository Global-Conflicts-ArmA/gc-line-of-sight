class GC_LineOfSightUI : SCR_MapUIBaseComponent
{
	// this is basically responsible for:
	// - ui for activating the tool
	// - controlling tool options
	// - setting starting position with cursor
	
	// beyond that, it just makes calls to the system
	
	
	protected SCR_MapToolEntry m_ToolMenuEntry;
	protected SCR_MapCursorModule m_CursorModule;
	protected GC_TracingSystem m_TracingSystem;
	
	protected bool m_bToolActive = false;
	
	protected float m_fSourceOffset = 1.5;
	protected float m_fTargetOffset = 0.5;
	
	protected float m_fSourceX, m_fSourceY;
	
	protected const ResourceName GUI_LAYOUT = "{B4D66F006C21C69E}UI/layouts/Menus/DeployMenu/GC_LineOfSightSettings.layout";
	protected const ResourceName TOOL_IMAGESET = "{3262679C50EF4F01}UI/Textures/Icons/icons_wrapperUI.imageset";
	protected const string TOOL_ICON = "terrainIcon";
	
	
	// GUI widgets
	protected Widget m_wLineOfSightRoot;
	protected EditBoxWidget m_wSourceBox;
	protected EditBoxWidget m_wTargetBox;
	protected ButtonWidget m_wShadingBox;
	protected ButtonWidget m_wHideButton;
	protected TextWidget m_wHideText;
	protected ButtonWidget m_wPositionButton;
	protected TextWidget m_wPositionText;
	protected TextWidget m_wStatusText;
	protected ButtonWidget m_wResolutionSlider;
	
	protected bool m_bAwaitingInputClick = false;
	protected bool m_bSystemActive = false;
	
	protected bool m_bHidePolygons = false;
	protected GC_ShadingMode m_ShadingMode = GC_ShadingMode.Darken;
	
	
	override void Init()
	{
		super.Init();
		
		SCR_MapToolMenuUI toolMenu = SCR_MapToolMenuUI.Cast(m_MapEntity.GetMapUIComponent(SCR_MapToolMenuUI));
		if (toolMenu)
		{
			m_ToolMenuEntry = toolMenu.RegisterToolMenuEntry(TOOL_IMAGESET, TOOL_ICON, 50);
			m_ToolMenuEntry.m_OnClick.Insert(ToolButtonClicked);
			m_ToolMenuEntry.SetEnabled(true);
		}
		
		m_CursorModule = SCR_MapCursorModule.Cast(m_MapEntity.GetMapModule(SCR_MapCursorModule));
		
		m_TracingSystem = GC_TracingSystem.Cast(GetGame().GetWorld().FindSystem(GC_TracingSystem));
	}
	
	protected void ToolButtonClicked()
	{
		m_bToolActive = !m_bToolActive;
		
		m_ToolMenuEntry.SetActive(m_bToolActive);
		
		if (!m_wLineOfSightRoot)
			CreateLayout();
		m_wLineOfSightRoot.SetVisible(m_bToolActive);
		
		if (m_bToolActive)
		{
			UpdateLayoutPosition();
			StartAwaitInput();
		}
		else
		{
			if (m_bSystemActive)
			{
				m_TracingSystem.DeactivateTool();
				m_bSystemActive = false;
			}
			if (m_bAwaitingInputClick)
				StopAwaitInput();
		}
	}
	
	protected void UpdateLayoutPosition()
	{
		if (!m_ToolMenuEntry || !m_ToolMenuEntry.m_ButtonComp)
			return;

		Widget toolButton = m_ToolMenuEntry.m_ButtonComp.GetRootWidget();
		if (!toolButton)
			return;
		
		WorkspaceWidget workspace = GetGame().GetWorkspace();
		if (!workspace)
			return;

		float buttonPosX, buttonPosY, buttonSizeX, buttonSizeY;
		toolButton.GetScreenPos(buttonPosX, buttonPosY);
		toolButton.GetScreenSize(buttonSizeX, buttonSizeY);
		
		FrameSlot.SetPosY(m_wLineOfSightRoot, workspace.DPIUnscale(buttonPosY - buttonSizeY + 40));  // improve positioning
		FrameSlot.SetPosX(m_wLineOfSightRoot, workspace.DPIUnscale(buttonPosX + buttonSizeX));
	}
	
	override void OnMapClose(MapConfiguration config)
	{		
		super.OnMapClose(config);
		
		if (m_bToolActive)
			ToolButtonClicked();
	}
	
	
	protected void StartAwaitInput()
	{
		m_bAwaitingInputClick = true;
		GetGame().GetInputManager().AddActionListener("MapSelect", EActionTrigger.DOWN, OnInputClick);
		m_wPositionText.SetText("Awaiting position");
	}
	
	protected void StopAwaitInput()
	{
		m_bAwaitingInputClick = false;
		GetGame().GetInputManager().RemoveActionListener("MapSelect", EActionTrigger.DOWN, OnInputClick);
		m_wPositionText.SetText("New position");
	}
	
	protected void OnInputClick(float value, EActionTrigger reason)
	{
		StopAwaitInput();
		
		SCR_UISoundEntity.SoundEvent(SCR_SoundEvent.SOUND_MAP_GADGET_MARKER_DRAW_START);
		
		m_MapEntity.GetMapCursorWorldPosition(m_fSourceX, m_fSourceY);
		m_TracingSystem.ActivateTool(m_fSourceX, m_fSourceY, m_fSourceOffset, m_fTargetOffset, m_wStatusText);
		m_bSystemActive = true;
	}
	
	protected void CreateLayout()
	{
		m_wLineOfSightRoot = GetGame().GetWorkspace().CreateWidgets(GUI_LAYOUT, m_RootWidget);
		m_wSourceBox = EditBoxWidget.Cast(m_wLineOfSightRoot.FindAnyWidget("SourceBox"));
		m_wTargetBox = EditBoxWidget.Cast(m_wLineOfSightRoot.FindAnyWidget("TargetBox"));
		m_wShadingBox = ButtonWidget.Cast(m_wLineOfSightRoot.FindAnyWidget("ShadingBox"));
		m_wHideButton = ButtonWidget.Cast(m_wLineOfSightRoot.FindAnyWidget("HideButton"));
		m_wHideText = TextWidget.Cast(m_wLineOfSightRoot.FindAnyWidget("HideText"));
		m_wPositionButton = ButtonWidget.Cast(m_wLineOfSightRoot.FindAnyWidget("PositionButton"));
		m_wPositionText = TextWidget.Cast(m_wLineOfSightRoot.FindAnyWidget("PositionText"));
		m_wStatusText = TextWidget.Cast(m_wLineOfSightRoot.FindAnyWidget("Status"));
		m_wResolutionSlider = ButtonWidget.Cast(m_wLineOfSightRoot.FindAnyWidget("ResolutionSlider"));
		
		SCR_ButtonComponent hideButtonComp = SCR_ButtonComponent.Cast(m_wHideButton.FindHandler(SCR_ButtonComponent));
		hideButtonComp.m_OnClicked.Insert(HideButtonClicked);
		
		SCR_ButtonComponent positionButtonComp = SCR_ButtonComponent.Cast(m_wPositionButton.FindHandler(SCR_ButtonComponent));
		positionButtonComp.m_OnClicked.Insert(PositionButtonClicked);
		
		EditBoxFilterComponent sourceBoxFilterComp = EditBoxFilterComponent.Cast(m_wSourceBox.FindHandler(EditBoxFilterComponent));
		sourceBoxFilterComp.m_OnValidInput.Insert(OffsetBoxInput);
		
		EditBoxFilterComponent targetBoxFilterComp = EditBoxFilterComponent.Cast(m_wTargetBox.FindHandler(EditBoxFilterComponent));
		targetBoxFilterComp.m_OnValidInput.Insert(OffsetBoxInput);
		
		SCR_ComboBoxComponent comboBoxComp = SCR_ComboBoxComponent.Cast(m_wShadingBox.FindHandler(SCR_ComboBoxComponent));
		comboBoxComp.m_OnChanged.Insert(ComboBoxChanged);
		
		SCR_SliderComponent sliderComp = SCR_SliderComponent.Cast(m_wResolutionSlider.FindHandler(SCR_SliderComponent));
		sliderComp.m_OnChanged.Insert(ResolutionSliderChanged);
		
#ifdef WORKBENCH
		m_wStatusText.SetVisible(true);
#endif
	}
	
	protected void PositionButtonClicked()
	{
		if (!m_bAwaitingInputClick)
			StartAwaitInput();
		else
			StopAwaitInput();
	}
	
	protected void OffsetBoxInput()
	{
		const string sourceText = m_wSourceBox.GetText();
		const string targetText = m_wTargetBox.GetText();
		
		if (!sourceText || !targetText)
			return;
		
		float sourceValue = sourceText.ToFloat(-1);
		float targetValue = targetText.ToFloat(-1);
		
		if (sourceValue < 0 || targetValue < 0)
		{
			m_wSourceBox.SetText(m_fSourceOffset.ToString());
			m_wTargetBox.SetText(m_fTargetOffset.ToString());
			return;
		}
		
		m_fSourceOffset = sourceValue;
		m_fTargetOffset = targetValue;
		
		if (m_bSystemActive)
			m_TracingSystem.ActivateTool(m_fSourceX, m_fSourceY, m_fSourceOffset, m_fTargetOffset, m_wStatusText);
	}
	
	protected void HideButtonClicked()
	{
		m_bHidePolygons = !m_bHidePolygons;
		if (m_bHidePolygons)
			m_wHideText.SetText("Unhide");
		else
			m_wHideText.SetText("Hide");
		UpdateColor();
	}
	
	protected void ResolutionSliderChanged()
	{
		SCR_SliderComponent sliderComp = SCR_SliderComponent.Cast(m_wResolutionSlider.FindHandler(SCR_SliderComponent));
		m_TracingSystem.SetResolutionMultiplier(sliderComp.GetValue());
	}
	
	protected void ComboBoxChanged()
	{
		SCR_ComboBoxComponent comboBoxComp = SCR_ComboBoxComponent.Cast(m_wShadingBox.FindHandler(SCR_ComboBoxComponent));
		
		string selection = comboBoxComp.GetCurrentItem();
		if (selection == "Darken")
			m_ShadingMode = GC_ShadingMode.Darken;
		else if (selection == "Blacken")
			m_ShadingMode = GC_ShadingMode.Blacken;
		else if (selection == "Colorize")
			m_ShadingMode = GC_ShadingMode.Obstacle;
		else if (selection == "Debug")
			m_ShadingMode = GC_ShadingMode.DebugVis;
		
		
		UpdateColor();
	}
	
	protected void UpdateColor()
	{
		if (m_bHidePolygons)
			m_TracingSystem.SetShadingMode(GC_ShadingMode.Hide);
		else
			m_TracingSystem.SetShadingMode(m_ShadingMode);
	}
}

enum GC_ShadingMode // must have same order as layout entries
{
	Hide,
	Darken,
	Blacken,
	Obstacle,
	DebugVis
}